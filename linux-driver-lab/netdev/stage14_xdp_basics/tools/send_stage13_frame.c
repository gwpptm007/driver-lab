// SPDX-License-Identifier: GPL-2.0
#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#define STAGE13_MAGIC "STAGE13"

static uint16_t csum16(const void *buf, size_t len)
{
    const uint16_t *p = buf;
    uint32_t sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(const uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
    return (uint16_t)(~sum);
}

int main(int argc, char **argv)
{
    const char *ifname = "nds13s";
    int count = 64;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--ifname") && i + 1 < argc) ifname = argv[++i];
        else if (!strcmp(argv[i], "--count") && i + 1 < argc) count = atoi(argv[++i]);
    }

    int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0) { perror("socket"); return 1; }

    struct ifreq ifr = {0};
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) { perror("SIOCGIFINDEX"); return 2; }
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) { perror("SIOCGIFHWADDR"); return 3; }

    unsigned char src_mac[ETH_ALEN];
    memcpy(src_mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    unsigned char dst_mac[ETH_ALEN] = {0x02,0xaa,0xbb,0xcc,0xdd,0xef};

    struct sockaddr_ll sll = {0};
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_halen = ETH_ALEN;
    memcpy(sll.sll_addr, dst_mac, ETH_ALEN);

    unsigned char frame[256] = {0};
    struct ethhdr *eth = (struct ethhdr *)frame;
    struct iphdr *iph = (struct iphdr *)(frame + sizeof(*eth));
    struct udphdr *uh = (struct udphdr *)(frame + sizeof(*eth) + sizeof(*iph));
    unsigned char *payload = frame + sizeof(*eth) + sizeof(*iph) + sizeof(*uh);
    const int payload_len = 32;

    memcpy(eth->h_source, src_mac, ETH_ALEN);
    memcpy(eth->h_dest, dst_mac, ETH_ALEN);
    eth->h_proto = htons(ETH_P_IP);

    iph->version = 4;
    iph->ihl = 5;
    iph->ttl = 64;
    iph->protocol = IPPROTO_UDP;
    iph->tot_len = htons(sizeof(*iph) + sizeof(*uh) + payload_len);
    iph->daddr = htonl(0x0a000002);

    uh->dest = htons(13013);
    uh->len = htons(sizeof(*uh) + payload_len);

    for (int i = 0; i < count; ++i) {
        iph->id = htons((uint16_t)i);
        iph->saddr = htonl(0x0a000100 + (i & 0xff));
        iph->check = 0;
        iph->check = csum16(iph, sizeof(*iph));
        uh->source = htons(12000 + (i % 8));
        uh->check = 0;
        memset(payload, 0, payload_len);
        memcpy(payload, STAGE13_MAGIC, sizeof(STAGE13_MAGIC)-1);
        payload[16] = (unsigned char)i;
        int frame_len = sizeof(*eth) + sizeof(*iph) + sizeof(*uh) + payload_len;
        if (sendto(fd, frame, frame_len, 0, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
            perror("sendto");
            close(fd);
            return 4;
        }
    }

    printf("sent %d frame(s), proto=0x0800, magic=%s\n", count, STAGE13_MAGIC);
    close(fd);
    return 0;
}
