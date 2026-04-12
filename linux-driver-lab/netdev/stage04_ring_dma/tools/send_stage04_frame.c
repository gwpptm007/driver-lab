// SPDX-License-Identifier: GPL-2.0
/*
 * send_stage04_frame.c — 构造并发送任意 ethertype 的 Ethernet 帧
 *
 * 【用途】
 *   用于 smoke test：向 netdev_stage04 发送 ethertype=0x88B7 的帧，
 *   验证 TX/RX path、ring 操作、NAPI poll 是否正常工作。
 *
 * 【frame 格式】
 *   本工具构造完整 Ethernet frame，手动填充所有字段：
 *     [0-5]   dst MAC     → ff:ff:ff:ff:ff:ff（广播）
 *     [6-11]  src MAC     → 24:24:24:24:24:24（模拟本地 MAC）
 *     [12-13] ethertype   → 用户指定（如 0x88B7）
 *     [14...] payload     → 用户指定（如 "xyz123"）
 *
 * 【关键设计决策：为什么用 ETH_P_ALL 而不是特定 ethertype？】
 *
 *   AF_PACKET SOCK_RAW 的行为取决于第三个协议参数：
 *   - ETH_P_ALL：接收/发送所有 Ethernet 帧，需要自己构造完整 frame
 *   - ETH_P_XXX（特定值）：kernel 自动处理 Ethernet header
 *     * 发送时：kernel 在 sendto 数据前自动 prepend Ethernet header
 *     * 接收时：只接收匹配 ethertype 的帧
 *
 *   如果绑定特定 ethertype 再 sendto 完整 frame，
 *   kernel 会再加一层 header，导致双层 Ethernet header！
 *   所以本工具使用 ETH_P_ALL + 手动完整 frame。
 *
 * 【用法示例】
 *   ./send_stage04_frame nds4 xyz123 88B7 3
 *     → 在 nds4 上发送 3 帧，ethertype=0x88B7，payload="xyz123"
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
#include <unistd.h>

#define DEFAULT_ETHERTYPE 0x88B7  /* 实验私有协议 ethertype */
#define MAX_FRAME_SIZE 1514       /* 标准 Ethernet MTU */

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s <ifname> [payload] [ethertype] [count] [interval_us]\n"
		"  <ifname>      : 目标接口名（如 nds4）\n"
		"  [payload]     : 可选负载数据（默认: stage04-default-payload）\n"
		"  [ethertype]   : 可选 ETHERTYPE（默认: 0x88B7，支持 hex 如 88B7）\n"
		"  [count]       : 可选 burst 数量（默认: 1）\n"
		"  [interval_us] : 两帧之间的 us 间隔（默认: 0）\n",
		prog);
}

int main(int argc, char **argv)
{
	int fd = -1;
	struct ifreq ifr;
	struct sockaddr_ll addr;
	unsigned char frame[MAX_FRAME_SIZE];
	const char *ifname;
	const char *payload = "stage04-default-payload";
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
		ethertype = (unsigned int)strtoul(argv[3], NULL, 16);  /* 必须用 16，strtoul(...,0) 会把 "88B7" 当 decimal 解析为 88 */
	if (argc >= 5)
		count = (unsigned int)strtoul(argv[4], NULL, 0);
	if (argc >= 6)
		interval_us = (unsigned int)strtoul(argv[5], NULL, 0);
	if (count == 0)
		count = 1;

	payload_len = strlen(payload);
	if (payload_len > MAX_FRAME_SIZE - ETH_HLEN) {
		fprintf(stderr, "payload too large (max %d bytes)\n", MAX_FRAME_SIZE - ETH_HLEN);
		return 1;
	}

	/*
	 * 【关键设计：为什么用 ETH_P_ALL 而不是特定 ethertype？】
	 *
	 * AF_PACKET SOCK_RAW 的第三个参数决定 socket 行为：
	 *
	 *   ETH_P_ALL：发送/接收所有 Ethernet 帧，需要手动构造完整 frame
	 *
	 *   ETH_P_XXX（特定值）：
	 *     * 发送时：kernel 在 sendto 数据前自动 prepend Ethernet header
	 *     * 接收时：只接收该 ethertype 的帧
	 *
	 * 本工具需要手动控制完整 Ethernet frame，所以必须用 ETH_P_ALL。
	 * 如果误用特定 ethertype + 传入完整 frame，kernel 会再加一层 header，
	 * 导致 RX 端看到双层 Ethernet header，ethertype 解析错误。
	 */
	fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd < 0) {
		perror("socket(AF_PACKET, SOCK_RAW, ETH_P_ALL)");
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
	memset(addr.sll_addr, 0xff, ETH_ALEN);  /* 广播：驱动能收到 */

	/*
	 * 构造完整 Ethernet frame：
	 *   [0-5]   dst MAC      → ff:ff:ff:ff:ff:ff（广播）
	 *   [6-11]  src MAC      → 24:24:24:24:24:24（模拟本地 MAC）
	 *   [12-13] ethertype    → 用户指定（0x88B7）
	 *   [14...] payload      → 用户指定
	 */
	memset(frame, 0, sizeof(frame));
	memset(frame, 0xff, ETH_ALEN);                      /* dst MAC = 广播 */
	memset(frame + ETH_ALEN, 0x24, ETH_ALEN);            /* src MAC */
	frame[12] = (unsigned char)((ethertype >> 8) & 0xff); /* ethertype 高字节 */
	frame[13] = (unsigned char)(ethertype & 0xff);       /* ethertype 低字节 */
	memcpy(frame + ETH_HLEN, payload, payload_len);      /* payload */
	frame_len = ETH_HLEN + payload_len;

	/* 发送 count 帧（可选 interval_us 间隔） */
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

	printf("[send_stage04_frame] sent %u frame(s) on %s ethertype=0x%04x payload=\"%s\"\n",
	       count, ifname, ethertype & 0xffff, payload);
	close(fd);
	return 0;
}
