// SPDX-License-Identifier: GPL-2.0
/*
 * recv_stage04_frame.c — 接收指定 ethertype 的 Ethernet 帧
 *
 * 【用途】
 *   与 send_stage04_frame 配套使用：发送端发帧，接收端收帧，
 *   验证 RX path 是否正常工作（RX count、protocol 解析等）。
 *
 * 【与 send_stage04_frame 的区别】
 *   - send：使用 ETH_P_ALL + 手动构造完整 frame
 *   - recv：绑定特定 ethertype，只接收匹配的帧（简化处理）
 *
 * 【recvfrom 返回的数据】
 *   recvfrom 会收到完整的 Ethernet frame（包含 header）：
 *     [0-5]   dst MAC
 *     [6-11]  src MAC
 *     [12-13] ethertype
 *     [14...] payload
 *   所以 payload 起始位置是 ETH_HLEN（14 字节）。
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

#define DEFAULT_ETHERTYPE 0x88B7
#define MAX_FRAME_SIZE 2048

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s <ifname> [ethertype] [max_frames] [timeout_sec]\n"
		"  <ifname>     : 目标接口名（如 nds4）\n"
		"  [ethertype]  : 可选 ETHERTYPE（默认: 0x88B7）\n"
		"  [max_frames] : 最多接收多少帧（默认: 1）\n"
		"  [timeout_sec]: 超时时间（默认: 5）\n",
		prog);
}

static const char *pkttype_to_str(unsigned int pkttype)
{
	switch (pkttype) {
	case PACKET_HOST: return "PACKET_HOST";
	case PACKET_OUTGOING: return "PACKET_OUTGOING";
	case PACKET_BROADCAST: return "PACKET_BROADCAST";
	case PACKET_MULTICAST: return "PACKET_MULTICAST";
	default: return "PACKET_OTHER";
	}
}

int main(int argc, char **argv)
{
	int fd = -1;
	int one = 1;
	struct ifreq ifr;
	struct sockaddr_ll bind_addr;
	unsigned char frame[MAX_FRAME_SIZE + 1];
	const char *ifname;
	unsigned int ethertype = DEFAULT_ETHERTYPE;
	unsigned int max_frames = 1;
	unsigned int timeout_sec = 5;
	unsigned int received = 0;
	struct sockaddr_ll rx_addr;
	socklen_t rx_addr_len;
	struct timeval tv;

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
	if (max_frames == 0)
		max_frames = 1;

	/* 绑定特定 ethertype 的 socket：只接收匹配的帧（与 send 不同） */
	fd = socket(AF_PACKET, SOCK_RAW, htons((unsigned short)ethertype));
	if (fd < 0) {
		perror("socket(AF_PACKET, SOCK_RAW)");
		return 1;
	}

	/* PACKET_IGNORE_OUTGOING：忽略本机发出的帧（避免回环） */
	(void)setsockopt(fd, SOL_PACKET, PACKET_IGNORE_OUTGOING, &one, sizeof(one));

	/* 设置接收超时，避免一直阻塞 */
	tv.tv_sec = (long)timeout_sec;
	tv.tv_usec = 0;
	(void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	/* 获取 ifindex */
	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
		perror("ioctl(SIOCGIFINDEX)");
		close(fd);
		return 1;
	}

	/* bind 到指定 ethertype（只收该协议的帧） */
	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sll_family = AF_PACKET;
	bind_addr.sll_protocol = htons((unsigned short)ethertype);
	bind_addr.sll_ifindex = ifr.ifr_ifindex;
	if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
		perror("bind(AF_PACKET)");
		close(fd);
		return 1;
	}

	/* 循环接收最多 max_frames 帧 */
	while (received < max_frames) {
		ssize_t n;
		rx_addr_len = sizeof(rx_addr);
		n = recvfrom(fd, frame, MAX_FRAME_SIZE, 0,
			     (struct sockaddr *)&rx_addr, &rx_addr_len);
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;  /* 超时，正常退出 */
			perror("recvfrom");
			close(fd);
			return 1;
		}

		frame[n] = '\0';
		/* recvfrom 返回完整 Ethernet frame，payload 从 ETH_HLEN 开始 */
		printf("[recv_stage04_frame] #%u len=%zd ifindex=%d protocol=0x%04x pkt_type=%s payload=\"%s\"\n",
		       received + 1, n, rx_addr.sll_ifindex,
		       ntohs(rx_addr.sll_protocol), pkttype_to_str(rx_addr.sll_pkttype),
		       (n > ETH_HLEN) ? (char *)(frame + ETH_HLEN) : "");
		received++;
	}

	printf("[recv_stage04_frame] received %u frame(s) on %s ethertype=0x%04x\n",
	       received, ifname, ethertype & 0xffff);
	close(fd);
	return 0;
}
