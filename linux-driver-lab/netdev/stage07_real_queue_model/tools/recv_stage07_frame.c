// SPDX-License-Identifier: GPL-2.0
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

#define DEFAULT_ETHERTYPE 0x88B7
#define MAX_FRAME_SIZE 2048

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s <ifname> [ethertype] [max_frames] [timeout_sec]\n",
		prog);
}

int main(int argc, char **argv)
{
	int fd = -1;
	int one = 1;
	struct ifreq ifr;
	struct sockaddr_ll bind_addr;
	unsigned char frame[MAX_FRAME_SIZE + 1];
	struct sockaddr_ll rx_addr;
	socklen_t rx_addr_len;
	struct timeval tv;
	const char *ifname;
	unsigned int ethertype = DEFAULT_ETHERTYPE;
	unsigned int max_frames = 1;
	unsigned int timeout_sec = 5;
	unsigned int received = 0;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	ifname = argv[1];
	if (argc >= 3)
		ethertype = (unsigned int)strtoul(argv[2], NULL, 0);
	if (argc >= 4)
		max_frames = (unsigned int)strtoul(argv[3], NULL, 0);
	if (argc >= 5)
		timeout_sec = (unsigned int)strtoul(argv[4], NULL, 0);
	if (!max_frames)
		max_frames = 1;

	fd = socket(AF_PACKET, SOCK_RAW, htons((unsigned short)ethertype));
	if (fd < 0) {
		perror("socket(AF_PACKET)");
		return 1;
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
		return 1;
	}

	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sll_family = AF_PACKET;
	bind_addr.sll_protocol = htons((unsigned short)ethertype);
	bind_addr.sll_ifindex = ifr.ifr_ifindex;
	if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
		perror("bind(AF_PACKET)");
		close(fd);
		return 1;
	}

	while (received < max_frames) {
		ssize_t n;
		rx_addr_len = sizeof(rx_addr);
		n = recvfrom(fd, frame, MAX_FRAME_SIZE, 0,
			     (struct sockaddr *)&rx_addr, &rx_addr_len);
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			perror("recvfrom");
			close(fd);
			return 1;
		}
		frame[n] = '\0';
		printf("[recv_stage07_frame] #%u len=%zd ifindex=%d protocol=0x%04x payload=\"%s\"\n",
		       received + 1, n, rx_addr.sll_ifindex,
		       ntohs(rx_addr.sll_protocol),
		       (n > ETH_HLEN) ? (char *)(frame + ETH_HLEN) : "");
		received++;
	}

	printf("[recv_stage07_frame] received %u frame(s) on %s ethertype=0x%04x\n",
	       received, ifname, ethertype & 0xffff);
	close(fd);
	return 0;
}
