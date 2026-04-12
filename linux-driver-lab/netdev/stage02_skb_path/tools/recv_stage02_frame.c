// SPDX-License-Identifier: GPL-2.0
/*
 * recv_stage02_frame.c - 用户态原始套接字收包工具（stage02 专用）
 *
 * ==================== 文件概述 ====================
 *
 * stage02 的环回测试需要：发送一帧 → 收到环回的帧
 * send_stage02_frame 负责发送，recv_stage02_frame 负责接收。
 *
 * 【recv_stage02_frame 的特殊性】
 *   → 它 bind() 到 AF_PACKET 套接字，接收匹配 ETHERTYPE 的帧
 *   → 但 sendto() 发送的帧也会经过 recvfrom()
 *   → 所以需要 PACKET_IGNORE_OUTGOING 过滤掉"自己发出去的帧"
 *
 * 【学习焦点】
 *   1. AF_PACKET bind() 语义：只接收特定 ETHERTYPE 的帧
 *   2. PACKET_IGNORE_OUTGOING：过滤发送产生的回环
 *   3. recvfrom() 与 sockaddr_ll：获取接收到的帧元数据
 *   4. pkt_type 含义：PACKET_HOST / PACKET_OUTGOING 等
 *
 * ==================== 代码结构 ====================
 *
 *  1. 头部注释与 include（第1~50行）
 *  2. 宏定义（第52~56行）
 *  3. usage + pkttype_to_str（第58~82行）
 *  4. main 函数（第84~140行）
 */

/* ==================== 第1部分：头文件 ==================== */
/*
 * 【头文件选择说明】
 *
 * _GNU_SOURCE：
 *   → glibc 系统需要此宏
 *
 * <sys/socket.h>
 *   → socket() / recvfrom() / close()
 *   → AF_PACKET
 *
 * <linux/if_packet.h>
 *   → struct sockaddr_ll：接收地址结构
 *   → PACKET_IGNORE_OUTGOING：过滤发送产生的帧
 *
 * <net/ethernet.h>
 *   → ETH_ALEN / ETH_HLEN
 *
 * <net/if.h>
 *   → struct ifreq
 *   → SIOCGIFINDEX
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
#define DEFAULT_ETHERTYPE 0x88B5
#define MAX_FRAME_SIZE 2048  /* 稍大一些，接收有余量 */

/* ==================== 第3部分：usage + 辅助函数 ==================== */
/*
 * usage - 打印命令行用法
 *
 * 【参数说明】
 *   <ifname>     → 目标接口名（如 nds2）
 *   [ethertype]  → 可选 ETHERTYPE（默认: 0x88B5）
 */
static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s <ifname> [ethertype]\n"
		"  <ifname>    : 目标接口名（如 nds2）\n"
		"  [ethertype] : 可选 ETHERTYPE（默认: 0x88B5）\n"
		"\n"
		"Example: %s nds2 0x88B5\n",
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
	ssize_t n;
	int tries = 0;
	const char *ifname;
	unsigned int ethertype = DEFAULT_ETHERTYPE;
	struct sockaddr_ll rx_addr;
	socklen_t rx_addr_len = sizeof(rx_addr);

	/*
	 * 【参数解析】
	 *
	 * argv[0] = 程序名
	 * argv[1] = 接口名（必须）
	 * argv[2] = ethertype（可选）
	 */
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	ifname = argv[1];
	if (argc >= 3)
		ethertype = (unsigned int)strtoul(argv[2], NULL, 0);

	/*
	 * 【创建 AF_PACKET/SOCK_RAW 原始套接字】
	 *
	 * 与 send_stage02_frame 相同，但这里要 bind() 到指定接口。
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
	 *   → 但 stage02 是环回测试，所以必须过滤
	 */
	(void)setsockopt(fd, SOL_PACKET, PACKET_IGNORE_OUTGOING, &one, sizeof(one));

	/*
	 * 【获取接口索引】
	 *
	 * 与 send_stage02_frame 相同：
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
	bind_addr.sll_family  = AF_PACKET;
	bind_addr.sll_protocol = htons((unsigned short)ethertype);
	bind_addr.sll_ifindex = ifr.ifr_ifindex;

	if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
		perror("bind(AF_PACKET)");
		close(fd);
		return 1;
	}

	/*
	 * 【recvfrom() 接收帧 ★】
	 *
	 * recvfrom() 的特殊性：
	 *   → 不仅返回数据，还返回发送者地址（rx_addr）
	 *   → rx_addr 包含 pkt_type、protocol、ifindex 等元数据
	 *   → 这就是为什么能用 pkt_type 过滤 TX loopback
	 *
	 * 【循环接收逻辑】
	 *   → 如果收到 PACKET_OUTGOING（自己发的），继续收
	 *   → 最多重试 8 次，避免死循环
	 *   → 如果 8 次都是 OUTGOING，说明没有收到环回帧
	 */
	for (;;) {
		rx_addr_len = sizeof(rx_addr);
		n = recvfrom(fd, frame, MAX_FRAME_SIZE, 0,
			     (struct sockaddr *)&rx_addr, &rx_addr_len);

		if (n < 0) {
			perror("recvfrom");
			close(fd);
			return 1;
		}

		/*
		 * 【PACKET_OUTGOING 检查】
		 *
		 * 如果是发送产生的 loopback，继续收下一个。
		 * 否则认为是真正的环回帧。
		 */
		if (rx_addr.sll_pkttype != PACKET_OUTGOING)
			break;
		if (++tries > 8) {
			fprintf(stderr, "too many PACKET_OUTGOING frames, no injected RX observed\n");
			close(fd);
			return 1;
		}
	}

	/*
	 * 【输出接收结果】
	 *
	 * rx_addr 包含的元数据：
	 *   sll_pkttype  → 包类型（HOST / OUTGOING / BROADCAST / MULTICAST）
	 *   sll_protocol → ETHERTYPE（网络字节序）
	 *   sll_ifindex  → 接收接口索引
	 *
	 * frame[0..5] 包含目的 MAC
	 * frame[6..11] 包含源 MAC
	 * frame[12..13] 包含 ETHERTYPE
	 * frame[14..n-1] 包含 payload
	 */
	frame[n] = '\0';
	printf("received %zd bytes on %s (ifindex=%d, pkttype=%s, proto=0x%04x)\n",
	       n, ifname, ifr.ifr_ifindex,
	       pkttype_to_str(rx_addr.sll_pkttype),
	       ntohs(rx_addr.sll_protocol));

	/*
	 * 【输出 payload】
	 *
	 * ETH_HLEN = 14 = 6(MAC dst) + 6(MAC src) + 2(ETHERTYPE)
	 * frame[ETH_HLEN..n-1] 是纯 payload
	 */
	if (n > ETH_HLEN) {
		printf("payload=%s\n", frame + ETH_HLEN);
	}

	close(fd);
	return 0;
}

/*
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
 * 【用法3：发送+接收（SENDRECV）★★ stage02 使用这个】
 *   socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))
 *   ↓
 *   setsockopt(PACKET_IGNORE_OUTGOING)  ← 关键！
 *   ↓
 *   bind(ifindex, ETHERTYPE)
 *   ↓
 *   sendto() → 发送帧
 *   recvfrom() → 接收帧（过滤掉自己发的）
 *
 * ==================== 附录：stage02 完整测试流程 ====================
 *
 * 终端1（接收）：
 *   ./recv_stage02_frame nds2 0x88B5
 *   ↓
 *   recvfrom() 阻塞等待...
 *
 * 终端2（发送）：
 *   ./send_stage02_frame nds2 hello_stage02 0x88B5
 *   ↓
 *   sendto() 发送帧
 *
 * 终端1（收到）：
 *   received 35 bytes on nds2 (ifindex=6, pkttype=PACKET_HOST, proto=0x88b5)
 *   payload=hello_stage02
 *
 * 【流程解析】
 *   sendto() → ndo_start_xmit() → stage02_build_rx_skb()
 *   → netif_rx(rx_skb) → 协议栈 RX → recvfrom() 收到
 */
