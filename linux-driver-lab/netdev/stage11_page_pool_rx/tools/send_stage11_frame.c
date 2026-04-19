#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_COUNT 64
#define MAGIC "STAGE11"

/* IPv4 header for varying source IP (creates different tx hash per frame) */
struct ip4hdr {
    uint8_t  ver_ihl;     /* version(4) + ihl(5) */
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
    fprintf(stderr, "Usage: %s --ifname <ifname> [--count 64]\n", prog);
}

int main(int argc, char **argv)
{
    const char *ifname = NULL;
    unsigned count = DEFAULT_COUNT;
    int fd, i;
    struct ifreq ifr;
    unsigned char frame[ETH_FRAME_LEN];
    struct ether_header *eh = (struct ether_header *)frame;
    struct sockaddr_ll sa;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--ifname") && i + 1 < argc)
            ifname = argv[++i];
        else if (!strcmp(argv[i], "--count") && i + 1 < argc)
            count = strtoul(argv[++i], NULL, 0);
        else {
            usage(argv[0]);
            return 3;
        }
    }
    if (!ifname) {
        usage(argv[0]);
        return 3;
    }

    fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0) {
        perror("socket");
        return 4;
    }

    unsigned int ifindex = if_nametoindex(ifname);
    if (!ifindex) {
        perror("if_nametoindex");
        close(fd);
        return 4;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("SIOCGIFHWADDR");
        close(fd);
        return 4;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ALL);  /* 接收所有帧，由 send 决定发送类型 */
    sa.sll_ifindex = ifindex;
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("bind");
        close(fd);
        return 4;
    }

    /*
     * 构造可变源 IP 的帧，使 skb_tx_hash() 对每个帧产生不同 hash，
     * 从而分散到不同 subqueue（q0/q1）。
     * 帧结构：Ethernet(14) + IPv4(20) + Payload(28) = 62 bytes
     * Linux netdev 的 skb_tx_hash() 会用 (skb->data + ETH_HLEN) 即 IP 头计算 hash。
     */
    for (i = 0; i < (int)count; ++i) {
        struct payload p;
        struct ip4hdr iph;
        uint32_t saddr;
        ssize_t len;

        /* IP header：固定目的 IP（0.0.0.0），源 IP 随帧序号变化 */
        memset(&iph, 0, sizeof(iph));
        iph.ver_ihl = 0x45;        /* IPv4, IHL=5 (20 bytes) */
        iph.tos = 0;
        iph.tot_len = htons(sizeof(iph) + sizeof(p));
        iph.id = htons((uint16_t)i);
        iph.ttl = 64;
        iph.proto = 253;           /* 实验性协议，避免被路由处理 */
        /* 源 IP 在 192.168.100.1 基础上递增，制造不同 hash */
        saddr = htonl(0xC0A86401 + (uint32_t)i);  /* 192.168.100.1 + i */
        iph.saddr = saddr;
        iph.daddr = htonl(0x00000000);  /* 0.0.0.0 */
        iph.check = 0;  /* 简化的校验和，驱动不检查 */

        /* Payload */
        memset(&p, 0, sizeof(p));
        memcpy(p.magic, MAGIC, strlen(MAGIC));
        p.seq = htonl((uint32_t)i);
        p.total = htonl(count);

        /* 组装完整帧：Ethernet + IP + Payload */
        memset(frame, 0, ETH_FRAME_LEN);
        memcpy(eh->ether_dhost, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
        memcpy(eh->ether_shost, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
        eh->ether_type = htons(ETH_P_IP);  /* 0x0800，让 kernel 做 L3 hash */
        memcpy(frame + sizeof(*eh), &iph, sizeof(iph));
        memcpy(frame + sizeof(*eh) + sizeof(iph), &p, sizeof(p));
        len = sizeof(*eh) + sizeof(iph) + sizeof(p);

        if (send(fd, frame, len, 0) < 0) {
            perror("send");
            close(fd);
            return 5;
        }
    }
    printf("sent %u frame(s), proto=0x%04x, magic=%s, varied saddr for queue spread\n",
           count, ETH_P_IP & 0xffff, MAGIC);
    close(fd);
    return 0;
}