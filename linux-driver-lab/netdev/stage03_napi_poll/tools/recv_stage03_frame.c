// SPDX-License-Identifier: GPL-2.0
/*
 * recv_stage03_frame.c - 用户态原始套接字收包工具（stage03 专用）
 *
 * ==================== 文件概述 ====================
 *
 * stage03 的接收工具比 stage02 多支持：
 *   1. burst 接收（max_frames 参数）
 *   2. 超时控制（timeout_sec 参数）
 *   → 这样才能完整验证 NAPI 环回：发送 burst → 接收 burst
 *
 * 【recv_stage03_frame 的特殊性】
 *   → 它 bind() 到 AF_PACKET 套接字，接收匹配 ETHERTYPE 的帧
 *   → sendto() 发送的帧也会经过 recvfrom()
 *   → 所以需要 PACKET_IGNORE_OUTGOING 过滤掉"自己发出去的帧"
 *
 * 【学习焦点】
 *   1. AF_PACKET bind() 语义：只接收特定 ETHERTYPE 的帧
 *   2. PACKET_IGNORE_OUTGOING：过滤发送产生的回环
 *   3. recvfrom() 与 sockaddr_ll：获取接收到的帧元数据
 *   4. 超时控制：SO_RCVTIMEO
 *   5. pkt_type 含义：PACKET_HOST / PACKET_OUTGOING 等
 *
 * ==================== 与 stage02 的区别 ====================
 *
 * stage02：recvfrom() 只接收一帧，超时固定
 * stage03：支持 max_frames 次接收，支持 timeout_sec 超时控制
 *
 * 目的：配合 stage03 的 burst 发送测试
 *   → max_frames=32 接收 burst 发送的 32 帧
 *   → timeout_sec=5 避免在无帧时永久阻塞
 *
 * ==================== 代码结构 ====================
 *
 *  1. 头部注释与 include（第1~60行）
 *  2. 宏定义（第62~66行）
 *  3. usage + pkttype_to_str（第68~112行）
 *  4. main 函数（第114~200行）
 */

/* ==================== 第1部分：头文件 ==================== */
/*
 * 【头文件选择说明】
 *
 * _GNU_SOURCE：
 *   → glibc 系统需要此宏
 *
 * <sys/socket.h>
 *   → socket() / recvfrom() / close() / setsockopt()
 *   → AF_PACKET / SOL_PACKET / SO_RCVTIMEO
 *
 * <linux/if_packet.h>
 *   → struct sockaddr_ll：接收地址结构
 *   → PACKET_IGNORE_OUTGOING：过滤发送产生的帧
 *
 * <net/ethernet.h>
 *   → ETH_ALEN / ETH_HLEN
 *
 * <net/if.h>
 *   → struct ifreq / SIOCGIFINDEX
 *
 * <sys/time.h>
 *   → struct timeval：SO_RCVTIMEO 超时结构
 *
 * 【PACKET_IGNORE_OUTGOING 的故事】
 *   → 如果不设置这个，sendto() 发送的帧会在本机 loopback
 *   → recvfrom() 会先收到自己发出去的帧
 *   → 这个宏告诉内核：忽略发送产生的 loopback 帧
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/*
 * 【PACKET_IGNORE_OUTGOING 的兼容性】
 *
 * 这个宏在某些 glibc 版本中可能没定义。
 * 如果编译报错，手动定义即可：
 *   #ifndef PACKET_IGNORE_OUTGOING
 *   #define PACKET_IGNORE_OUTGOING 23
 *   #endif
 */
#ifndef PACKET_IGNORE_OUTGOING
#define PACKET_IGNORE_OUTGOING 23
#endif

/* ==================== 第2部分：宏定义 ==================== */
#define DEFAULT_ETHERTYPE 0x88B6
#define MAX_FRAME_SIZE 2048  /* 稍大一些，接收有余量 */

/* ==================== 第3部分：usage + 辅助函数 ==================== */
/*
 * usage - 打印命令行用法
 *
 * 【参数说明】
 *   <ifname>       → 目标接口名（如 nds3）
 *   [ethertype]    → 可选 ETHERTYPE（默认: 0x88B6）
 *   [max_frames]   → 最多接收多少帧（默认: 1）
 *   [timeout_sec]  → 超时时间（默认: 5）
 *
 * 【为什么要支持 max_frames？】
 *   → stage03 的 burst 测试需要一次接收多帧
 *   → max_frames=32 配合 send 的 burst=32
 *
 * 【为什么要支持 timeout_sec？】
 *   → 避免在无帧时永久阻塞
 *   → timeout=5 表示 5 秒无帧就退出
 */
static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s <ifname> [ethertype] [max_frames] [timeout_sec]\n"
		"  <ifname>     : 目标接口名（如 nds3）\n"
		"  [ethertype]  : 可选 ETHERTYPE（默认: 0x88B6）\n"
		"  [max_frames] : 最多接收多少帧（默认: 1）\n"
		"  [timeout_sec]: 超时时间（默认: 5）\n"
		"\n"
		"Example: %s nds3 0x88B6 32 5\n",
		prog, prog);
}

/*
 * pkttype_to_str - pkt_type 值转字符串
 *
 * 【pkt_type 的含义】
 *
 * PACKET_HOST (0)：
 *   → 发送给本机的帧（正常 RX）
 *
 * PACKET_OUTGOING (4)：
 *   → 从本机发出的帧（TX loopback）
 *   → 这就是为什么需要 PACKET_IGNORE_OUTGOING
 *
 * PACKET_BROADCAST (1)：
 *   → 广播帧
 *
 * PACKET_MULTICAST (2)：
 *   → 多播帧
 *
 * PACKET_OTHERHOST (3)：
 *   → 发往其他主机的帧（混杂模式可能收到）
 */
static const char *pkttype_to_str(unsigned int pkttype)
{
	switch (pkttype) {
	case PACKET_HOST:
		return "PACKET_HOST";
	case PACKET_OUTGOING:
		return "PACKET_OUTGOING";
	case PACKET_BROADCAST:
		return "PACKET_BROADCAST";
	case PACKET_MULTICAST:
		return "PACKET_MULTICAST";
	default:
		return "PACKET_OTHER";
	}
}

/* ==================== 第4部分：main 函数 ==================== */
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

	/*
	 * 【参数解析】
	 *
	 * argv[0] = 程序名
	 * argv[1] = 接口名（必须）
	 * argv[2] = ethertype（可选）
	 * argv[3] = max_frames（可选）
	 * argv[4] = timeout_sec（可选）
	 */
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

	/*
	 * 【创建 AF_PACKET/SOCK_RAW 原始套接字】
	 *
	 * 与 send_stage03_frame 相同，但这里要 bind() 到指定接口。
	 */
	fd = socket(AF_PACKET, SOCK_RAW, htons((unsigned short)ethertype));
	if (fd < 0) {
		perror("socket(AF_PACKET, SOCK_RAW)");
		return 1;
	}

	/*
	 * 【PACKET_IGNORE_OUTGOING - 过滤 TX loopback ★】
	 *
	 * 这个 setsockopt 非常关键：
	 *   → 不设置的话，sendto() 发送的帧会 loopback 到 recvfrom()
	 *   → 你会先收到自己发出去的帧，而不是环回的 RX 帧
	 *   → 这个选项告诉内核：忽略"从本机发出"的帧
	 *
	 * 【什么时候需要去掉这个过滤？】
	 *   → 测试真实网卡时
	 *   → 需要观察发送出去的帧时
	 *   → 但 stage03 是环回测试，所以必须过滤
	 */
	(void)setsockopt(fd, SOL_PACKET, PACKET_IGNORE_OUTGOING, &one, sizeof(one));

	/*
	 * 【SO_RCVTIMEO - 接收超时控制 ★】
	 *
	 * 设置 socket 接收超时：
	 *   → tv.tv_sec = timeout_sec
	 *   → 超时后 recvfrom() 返回 -1，errno = EAGAIN/EWOULDBLOCK
	 *   → 避免在无帧时永久阻塞
	 *
	 * 【为什么需要这个？】
	 *   → burst 发送可能很快，接收方需要等待
	 *   → 但如果网络有问题，不能无限等
	 *   → timeout=5 是合理的默认值
	 */
	tv.tv_sec = (long)timeout_sec;
	tv.tv_usec = 0;
	(void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	/*
	 * 【获取接口索引】
	 *
	 * 与 send_stage03_frame 相同：
	 *   ioctl(SIOCGIFINDEX) 把接口名转为 ifindex
	 */
	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
		perror("ioctl(SIOCGIFINDEX)");
		close(fd);
		return 1;
	}

	/*
	 * 【bind() 套接字到指定接口 ★】
	 *
	 * sendto() 和 recvfrom() 都可以指定目标接口，
	 * 但 bind() 的语义是"只接收来自这个接口的包"。
	 *
	 * bind() 后的 sockaddr_ll 字段：
	 *   sll_family    = AF_PACKET
	 *   sll_protocol  = ETHERTYPE（过滤条件）
	 *   sll_ifindex   = 接口索引（过滤条件）
	 *   sll_pkttype   = 0（暂时不知道）
	 *   sll_addr      = 0（不关心 MAC）
	 *
	 * 【为什么需要 bind？】
	 *   → AF_PACKET 套接字可以绑定到多个接口（通过多个 bind）
	 *   → 或者绑定到特定 ETHERTYPE
	 *   → 不 bind 的话，会收到所有匹配 protocol 的包
	 *   → bind 提供了更精确的过滤
	 */
	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sll_family = AF_PACKET;
	bind_addr.sll_protocol = htons((unsigned short)ethertype);
	bind_addr.sll_ifindex = ifr.ifr_ifindex;
	if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
		perror("bind(AF_PACKET)");
		close(fd);
		return 1;
	}

	/*
	 * 【recvfrom() 循环接收帧 ★】
	 *
	 * 接收逻辑：
	 *   → 循环直到收到 max_frames 帧
	 *   → 每次 recvfrom() 最多接收一帧
	 *   → 超时后退出（EAGAIN/EWOULDBLOCK）
	 *
	 * 【输出格式】
	 *   每收到一帧输出：
	 *     [#序号] len=XX ifindex=X protocol=0xXXXX pkt_type=XXX payload="XXX"
	 */
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
		printf("[recv_stage03_frame] #%u len=%zd ifindex=%d protocol=0x%04x pkt_type=%s payload=\"%s\"\n",
		       received + 1, n, rx_addr.sll_ifindex,
		       ntohs(rx_addr.sll_protocol), pkttype_to_str(rx_addr.sll_pkttype),
		       (n > ETH_HLEN) ? (char *)(frame + ETH_HLEN) : "");
		received++;
	}

	printf("[recv_stage03_frame] received %u frame(s) on %s ethertype=0x%04x\n",
	       received, ifname, ethertype & 0xffff);
	close(fd);
	return 0;
}

/*
 * ==================== 附录：stage03 测试场景 ====================
 *
 * 【场景1：基本环回验证】
 *   终端1（接收）：./recv_stage03_frame nds3 0x88B6 1 5
 *   终端2（发送）：./send_stage03_frame nds3 hello 0x88B6 1 0
 *   → 收到 1 帧，pkt_type=PACKET_HOST
 *
 * 【场景2：burst 环回验证】
 *   终端1（接收）：./recv_stage03_frame nds3 0x88B6 32 5
 *   终端2（发送）：./send_stage03_frame nds3 hello 0x88B6 32 0
 *   → 收到 32 帧，验证 NAPI 批量处理
 *
 * 【场景3：超时退出】
 *   终端1（接收）：./recv_stage03_frame nds3 0x88B6 10 2
 *   → 2 秒无帧则退出，即使没收到 10 帧
 *
 * ==================== 附录：AF_PACKET 套接字的三种用法 ====================
 *
 * 【用法1：发送（SENDONLY）】
 *   socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))
 *   ↓
 *   sendto(ifindex, frame)
 *   → 可以发到任意接口，不绑定
 *
 * 【用法2：接收（RECVONLY）】
 *   socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))
 *   ↓
 *   bind(ifindex, ETH_P_ALL)
 *   ↓
 *   recvfrom()
 *   → 只接收指定接口/协议的包
 *
 * 【用法3：发送+接收（SENDRECV）★★ stage03 使用这个】
 *   socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))
 *   ↓
 *   setsockopt(PACKET_IGNORE_OUTGOING)  ← 关键！
 *   ↓
 *   bind(ifindex, ETHERTYPE)
 *   ↓
 *   sendto() → 发送帧
 *   recvfrom() → 接收帧（过滤掉自己发的）
 *
 * ==================== 附录：stage03 完整测试流程 ====================
 *
 * 终端1（接收）：
 *   ./recv_stage03_frame nds3 0x88B6 32 5
 *   ↓
 *   recvfrom() 阻塞等待...
 *
 * 终端2（发送）：
 *   ./send_stage03_frame nds3 hello 0x88B6 32 0
 *   ↓
 *   sendto() 发送 burst
 *
 * 终端1（收到 32 帧）：
 *   [recv_stage03_frame] #1 len=35 ifindex=10 protocol=0x88b6 pkt_type=PACKET_HOST payload="hello"
 *   ...（共 32 帧）
 *   [recv_stage03_frame] received 32 frame(s) on nds3 ethertype=0x88b6
 *
 * 【流程解析（napi 模式）】
 *   sendto() → ndo_start_xmit() → stage03_build_rx_skb()
 *   → skb_queue_tail(pending_rxq) → stage03_raise_irq()
 *   → napi_schedule() → stage03_napi_poll()
 *   → skb_dequeue() → netif_receive_skb()
 *   → 协议栈 RX → recvfrom() 收到
 */
