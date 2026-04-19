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

#define DEFAULT_PROTO 0x88B9
#define DEFAULT_COUNT 64
#define MAGIC "STAGE09"

struct payload {
    char magic[8];
    uint32_t seq;
    uint32_t total;
};

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s --ifname <ifname> [--proto 0x88B9] [--count 64]\n", prog);
}

int main(int argc, char **argv)
{
    const char *ifname = NULL;
    unsigned proto = DEFAULT_PROTO;
    unsigned count = DEFAULT_COUNT;
    int fd, i;
    struct ifreq ifr;
    unsigned char frame[ETH_FRAME_LEN];
    struct ether_header *eh = (struct ether_header *)frame;
    struct sockaddr_ll sa;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--ifname") && i + 1 < argc)
            ifname = argv[++i];
        else if (!strcmp(argv[i], "--proto") && i + 1 < argc)
            proto = strtoul(argv[++i], NULL, 0);
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

    fd = socket(AF_PACKET, SOCK_RAW, htons((uint16_t)proto));
    if (fd < 0) {
        perror("socket");
        return 4;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        close(fd);
        return 4;
    }
    memset(frame, 0, sizeof(frame));
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("SIOCGIFHWADDR");
        close(fd);
        return 4;
    }
    memcpy(eh->ether_dhost, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    memcpy(eh->ether_shost, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    eh->ether_type = htons((uint16_t)proto);

    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_ifindex = ifr.ifr_ifindex;
    sa.sll_halen = ETH_ALEN;
    memcpy(sa.sll_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

    for (i = 0; i < (int)count; ++i) {
        struct payload p;
        ssize_t len;
        memset(&p, 0, sizeof(p));
        memcpy(p.magic, MAGIC, strlen(MAGIC));
        p.seq = htonl((uint32_t)i);
        p.total = htonl(count);
        memcpy(frame + sizeof(*eh), &p, sizeof(p));
        len = sizeof(*eh) + sizeof(p);
        if (sendto(fd, frame, len, 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
            perror("sendto");
            close(fd);
            return 5;
        }
    }
    printf("sent %u frame(s), proto=0x%04x, magic=%s\n", count, proto & 0xffff, MAGIC);
    close(fd);
    return 0;
}
