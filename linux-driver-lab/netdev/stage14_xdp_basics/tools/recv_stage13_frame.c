// SPDX-License-Identifier: GPL-2.0
#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define STAGE13_MAGIC "STAGE13"

int main(int argc, char **argv)
{
    const char *ifname = "nds13s";
    int timeout_sec = 5;
    int count = 1;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--ifname") && i + 1 < argc) ifname = argv[++i];
        else if (!strcmp(argv[i], "--timeout-sec") && i + 1 < argc) timeout_sec = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--count") && i + 1 < argc) count = atoi(argv[++i]);
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (fd < 0) { perror("socket"); return 4; }
    struct timeval tv = {.tv_sec = timeout_sec, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int ifindex = if_nametoindex(ifname);
    if (!ifindex) { perror("if_nametoindex"); return 4; }
    struct sockaddr_ll sll = {0};
    sll.sll_family = AF_PACKET; sll.sll_protocol = htons(ETH_P_IP); sll.sll_ifindex = ifindex;
    if (bind(fd, (struct sockaddr *)&sll, sizeof(sll)) < 0) { perror("bind"); return 4; }
    unsigned char buf[2048];
    int matched = 0;
    while (matched < count) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("recv"); close(fd); return 4;
        }
        if ((size_t)n < sizeof(struct ethhdr)+sizeof(struct iphdr)+sizeof(struct udphdr)+8) continue;
        struct ethhdr *eth = (struct ethhdr *)buf;
        if (ntohs(eth->h_proto) != ETH_P_IP) continue;
        struct iphdr *iph = (struct iphdr *)(buf + sizeof(*eth));
        if (iph->protocol != IPPROTO_UDP) continue;
        unsigned char *payload = buf + sizeof(*eth) + iph->ihl*4 + sizeof(struct udphdr);
        if (!memcmp(payload, STAGE13_MAGIC, sizeof(STAGE13_MAGIC)-1)) matched++;
    }
    printf("received %d frame(s), matched_proto=%d, matched_magic=%d, timeout=%d\n", matched, matched, matched, matched < count);
    close(fd);
    return matched >= count ? 0 : 2;
}
