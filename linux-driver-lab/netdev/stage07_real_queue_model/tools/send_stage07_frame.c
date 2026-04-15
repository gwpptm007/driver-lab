// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_ETHERTYPE 0x88B7
#define MAX_FRAME_SIZE 1514

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s <ifname> [payload] [ethertype] [count] [interval_us]\n",
		prog);
}

int main(int argc, char **argv)
{
	int fd = -1;
	struct ifreq ifr;
	struct sockaddr_ll addr;
	unsigned char frame[MAX_FRAME_SIZE];
	const char *ifname;
	const char *payload = "stage07-default-payload";
	unsigned int ethertype = DEFAULT_ETHERTYPE;
	unsigned int count = 1;
	unsigned int interval_us = 0;
	size_t payload_len;
	size_t frame_len;
	unsigned int i;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	ifname = argv[1];
	if (argc >= 3)
		payload = argv[2];
	if (argc >= 4)
		ethertype = (unsigned int)strtoul(argv[3], NULL, 16);
	if (argc >= 5)
		count = (unsigned int)strtoul(argv[4], NULL, 0);
	if (argc >= 6)
		interval_us = (unsigned int)strtoul(argv[5], NULL, 0);
	if (!count)
		count = 1;

	payload_len = strlen(payload);
	if (payload_len > MAX_FRAME_SIZE - ETH_HLEN) {
		fprintf(stderr, "payload too large\n");
		return 1;
	}

	fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd < 0) {
		perror("socket(AF_PACKET)");
		return 1;
	}

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
		perror("ioctl(SIOCGIFINDEX)");
		close(fd);
		return 1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = ifr.ifr_ifindex;
	addr.sll_halen = ETH_ALEN;
	memset(addr.sll_addr, 0xff, ETH_ALEN);

	memset(frame, 0, sizeof(frame));
	memset(frame, 0xff, ETH_ALEN);
	memset(frame + ETH_ALEN, 0x27, ETH_ALEN);
	frame[12] = (unsigned char)((ethertype >> 8) & 0xff);
	frame[13] = (unsigned char)(ethertype & 0xff);
	memcpy(frame + ETH_HLEN, payload, payload_len);
	frame_len = ETH_HLEN + payload_len;

	for (i = 0; i < count; ++i) {
		if (sendto(fd, frame, frame_len, 0,
			   (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("sendto");
			close(fd);
			return 1;
		}
		if (interval_us)
			usleep(interval_us);
	}

	printf("[send_stage07_frame] sent %u frame(s) on %s ethertype=0x%04x payload=\"%s\"\n",
	       count, ifname, ethertype & 0xffff, payload);
	close(fd);
	return 0;
}
