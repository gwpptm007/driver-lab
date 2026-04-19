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

#define DEFAULT_PROTO 0x88B9
#define DEFAULT_TIMEOUT 5
#define MAGIC "STAGE09"

struct payload {
    char magic[8];
    uint32_t seq;
    uint32_t total;
};

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s --ifname <ifname> [--proto 0x88B9] [--timeout-sec 5] [--count 1]\n", prog);
}

int main(int argc, char **argv)
{
    const char *ifname = NULL;
    unsigned proto = DEFAULT_PROTO;
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
        else if (!strcmp(argv[i], "--proto") && i + 1 < argc)
            proto = strtoul(argv[++i], NULL, 0);
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

    fd = socket(AF_PACKET, SOCK_RAW, htons((uint16_t)proto));
    if (fd < 0) {
        perror("socket");
        return 4;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons((uint16_t)proto);
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
                printf("received %u frame(s), matched_proto=%u, matched_magic=%u, timeout=1\n", matched, matched, matched);
                close(fd);
                return matched >= want ? 0 : 2;
            }
            perror("recv");
            close(fd);
            return 4;
        }
        if ((size_t)n < sizeof(struct ether_header) + sizeof(struct payload))
            continue;
        if (ntohs(((struct ether_header *)buf)->ether_type) != (uint16_t)proto)
            continue;
        if (memcmp(buf + sizeof(struct ether_header), MAGIC, strlen(MAGIC)) != 0)
            continue;
        printf("match proto=0x%04x magic=%s\n", proto & 0xffff, MAGIC);
        matched++;
    }
    printf("received %u frame(s), matched_proto=%u, matched_magic=%u, timeout=0\n", matched, matched, matched);
    close(fd);
    return 0;
}
