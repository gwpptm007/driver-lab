// SPDX-License-Identifier: GPL-2.0
/*
 * recv_stage08_frame.c — 接收并精确匹配 stage08 测试帧
 *
 * v2 版本的重点：
 * 1. 只把 ethertype=0x88B8 且 payload 前缀为 "STAGE08" 的帧计入成功
 * 2. 输出统一摘要行，供 smoke.sh 做硬判定
 * 3. 返回码有语义：
 *    0=达到预期帧数, 2=超时但数量不足, 3=参数错误, 4=socket/权限错误
 */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef PACKET_IGNORE_OUTGOING
#define PACKET_IGNORE_OUTGOING 23
#endif

#define DEFAULT_ETHERTYPE 0x88B8
#define MAX_FRAME_SIZE 2048
#define TEST_MAGIC "STAGE08"
#define TEST_MAGIC_LEN 7

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s <ifname> [ethertype] [expected_frames] [timeout_sec]\n",
        prog);
}

static int parse_seq(const unsigned char *payload, size_t payload_len)
{
    const char prefix[] = TEST_MAGIC " seq=";
    size_t i;
    int seq = -1;
    unsigned int v = 0;
    int seen_digit = 0;

    if (payload_len < sizeof(prefix) - 1)
        return -1;
    if (memcmp(payload, prefix, sizeof(prefix) - 1) != 0)
        return -1;

    for (i = sizeof(prefix) - 1; i < payload_len; ++i) {
        if (payload[i] < '0' || payload[i] > '9')
            break;
        seen_digit = 1;
        v = v * 10 + (unsigned int)(payload[i] - '0');
    }
    if (seen_digit)
        seq = (int)v;
    return seq;
}

int main(int argc, char **argv)
{
    int fd = -1;
    int one = 1;
    int rc = 0;
    struct ifreq ifr;
    struct sockaddr_ll bind_addr;
    unsigned char frame[MAX_FRAME_SIZE + 1];
    struct sockaddr_ll rx_addr;
    socklen_t rx_addr_len;
    struct timeval tv;
    const char *ifname;
    unsigned int ethertype = DEFAULT_ETHERTYPE;
    unsigned int expected_frames = 1;
    unsigned int timeout_sec = 5;
    unsigned int total_received = 0;
    unsigned int matched_proto = 0;
    unsigned int matched_magic = 0;

    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc < 2) {
        usage(argv[0]);
        return 3;
    }

    ifname = argv[1];
    if (argc >= 3)
        ethertype = (unsigned int)strtoul(argv[2], NULL, 0);
    if (argc >= 4)
        expected_frames = (unsigned int)strtoul(argv[3], NULL, 0);
    if (argc >= 5)
        timeout_sec = (unsigned int)strtoul(argv[4], NULL, 0);
    if (!expected_frames)
        expected_frames = 1;

    printf("recv start: ifname=%s proto=0x%04x expected=%u timeout=%u\n",
           ifname, ethertype & 0xffff, expected_frames, timeout_sec);

    fd = socket(AF_PACKET, SOCK_RAW, htons((unsigned short)ethertype));
    if (fd < 0) {
        perror("socket(AF_PACKET)");
        return 4;
    }

    (void)setsockopt(fd, SOL_PACKET, PACKET_IGNORE_OUTGOING, &one, sizeof(one));
    tv.tv_sec = (long)timeout_sec;
    tv.tv_usec = 0;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl(SIOCGIFINDEX)");
        close(fd);
        return 4;
    }

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sll_family = AF_PACKET;
    bind_addr.sll_protocol = htons((unsigned short)ethertype);
    bind_addr.sll_ifindex = ifr.ifr_ifindex;
    if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("bind(AF_PACKET)");
        close(fd);
        return 4;
    }

    while (matched_magic < expected_frames) {
        ssize_t n;
        unsigned short onwire_proto = 0;
        int seq = -1;
        const unsigned char *payload = NULL;
        size_t payload_len = 0;

        rx_addr_len = sizeof(rx_addr);
        n = recvfrom(fd, frame, MAX_FRAME_SIZE, 0,
                 (struct sockaddr *)&rx_addr, &rx_addr_len);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                rc = 2;
                break;
            }
            perror("recvfrom");
            close(fd);
            return 4;
        }

        total_received++;
        frame[n] = '\0';

        if (n >= ETH_HLEN) {
            onwire_proto = (unsigned short)(((unsigned short)frame[12] << 8) | frame[13]);
            payload = frame + ETH_HLEN;
            payload_len = (size_t)n - ETH_HLEN;
        }

        if (onwire_proto == (ethertype & 0xffff))
            matched_proto++;

        if (payload && payload_len >= TEST_MAGIC_LEN &&
            memcmp(payload, TEST_MAGIC, TEST_MAGIC_LEN) == 0) {
            seq = parse_seq(payload, payload_len);
            matched_magic++;
            printf("matched frame #%u len=%zd ifindex=%d protocol=0x%04x seq=%d payload=\"%.*s\"\n",
                   matched_magic, n, rx_addr.sll_ifindex, onwire_proto,
                   seq, (int)payload_len, (const char *)payload);
        } else {
            printf("ignored frame total=%u len=%zd ifindex=%d protocol=0x%04x\n",
                   total_received, n, rx_addr.sll_ifindex, onwire_proto);
        }
    }

    if (matched_magic >= expected_frames)
        rc = 0;

    printf("received %u frame(s), matched_proto=%u, matched_magic=%u, timeout=%u, expected=%u, proto=0x%04x\n",
           total_received, matched_proto, matched_magic,
           rc == 2 ? 1U : 0U, expected_frames, ethertype & 0xffff);
    fflush(stdout);
    close(fd);
    return rc;
}
