#include <arpa/inet.h>
#include <errno.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_TIMEOUT 5
#define MAGIC "STAGE12"

/* 与 send 保持一致：Ethernet(14) + IPv4(20) + Payload(28) = 62 bytes */
#define STAGE12_IP_PROTO 253       /* 帧携带的 IP 协议号（实验性） */
#define STAGE12_FRAME_TYPE ETH_P_IP  /* 0x0800，让 kernel 做 L3 hash */

struct ip4hdr {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
};

struct payload {
    char magic[8];
    uint32_t seq;
    uint32_t total;
};

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s --ifname <ifname> [--timeout-sec 5] [--count 1]\n", prog);
}

int main(int argc, char **argv)
{
    const char *ifname = NULL;
    unsigned timeout_sec = DEFAULT_TIMEOUT;
    unsigned want = 1;
    int fd;
    struct sockaddr_ll sa;
    struct timeval tv;
    unsigned matched = 0;
    unsigned char buf[2048];
    int i;

    setvbuf(stdout, NULL, _IOLBF, 0);
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--ifname") && i + 1 < argc)
            ifname = argv[++i];
        else if (!strcmp(argv[i], "--timeout-sec") && i + 1 < argc)
            timeout_sec = strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--count") && i + 1 < argc)
            want = strtoul(argv[++i], NULL, 0);
        else {
            usage(argv[0]);
            return 3;
        }
    }
    if (!ifname) {
        usage(argv[0]);
        return 3;
    }

    /* ETH_P_ALL 接收所有协议帧，bind 时指定 ifindex 过滤 */
    fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0) {
        perror("socket");
        return 4;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ALL);
    sa.sll_ifindex = if_nametoindex(ifname);
    if (!sa.sll_ifindex) {
        perror("if_nametoindex");
        close(fd);
        return 4;
    }
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("bind");
        close(fd);
        return 4;
    }
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt");
        close(fd);
        return 4;
    }

    while (matched < want) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("received %u frame(s), matched_proto=%u, matched_magic=%u, timeout=1\n",
                       matched, matched, matched);
                close(fd);
                return matched >= want ? 0 : 2;
            }
            perror("recv");
            close(fd);
            return 4;
        }
        /* 帧结构：Ethernet(14) + IPv4(20) + Payload(28) = 62 bytes */
        if ((size_t)n < ETH_HLEN + sizeof(struct ip4hdr) + sizeof(struct payload))
            continue;
        /* 检查 ether_type = IPv4 */
        if (ntohs(((struct ether_header *)buf)->ether_type) != ETH_P_IP)
            continue;
        /* 检查 IP 协议号 = 253 */
        struct ip4hdr *iph = (struct ip4hdr *)(buf + ETH_HLEN);
        if (iph->proto != STAGE12_IP_PROTO)
            continue;
        /* 检查 magic 在 IP 头之后 */
        struct payload *p = (struct payload *)(buf + ETH_HLEN + sizeof(struct ip4hdr));
        if (memcmp(p->magic, MAGIC, strlen(MAGIC)) != 0)
            continue;
        printf("match proto=0x%04x ip_proto=%u magic=%s\n",
               ETH_P_IP & 0xffff, iph->proto, MAGIC);
        matched++;
    }
    printf("received %u frame(s), matched_proto=%u, matched_magic=%u, timeout=0\n",
           matched, matched, matched);
    close(fd);
    return 0;
}