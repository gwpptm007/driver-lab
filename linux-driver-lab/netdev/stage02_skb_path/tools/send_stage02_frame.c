// SPDX-License-Identifier: GPL-2.0
/*
 * send_stage02_frame.c - 用户态原始套接字发包工具（stage02 专用）
 *
 * ==================== 文件概述 ====================
 *
 * stage01 和 stage02 都使用相同的发包工具，只是 ETHERTYPE 选择更灵活。
 * stage01 用固定的 0x88B5，stage02 支持自定义 ETHERTYPE。
 *
 * 【学习焦点】
 *   1. AF_PACKET / SOCK_RAW：链路层原始套接字
 *   2. sockaddr_ll：链路层地址结构
 *   3. ETHERTYPE 选择与协议栈解析
 *   4. SIOCGIFINDEX：接口名→ifindex 转换
 *   5. 帧格式：以太网头的每一字节
 *
 * ==================== 代码结构 ====================
 *
 *  1. 头部注释与 include（第1~50行）
 *  2. 宏定义（第52~56行）
 *  3. usage 说明（第58~68行）
 *  4. main 函数（第70~104行）
 *
 * 【与 send_stage01_frame.c 的区别】
 *   → 支持自定义 ETHERTYPE（命令行参数）
 *   → payload 默认值不同
 */

/* ==================== 第1部分：头文件 ==================== */
/*
 * 【头文件选择说明】
 *
 * _GNU_SOURCE：
 *   → glibc 系统需要此宏才能看到完整的 struct ifreq 定义
 *   → 必须在所有 include 之前定义
 *
 * <sys/socket.h>
 *   → socket() / sendto() / close()
 *   → AF_PACKET / SOCK_RAW
 *
 * <sys/ioctl.h>
 *   → ioctl()：设备控制
 *
 * <net/if.h>
 *   → struct ifreq：ioctl 接口名
 *   → SIOCGIFINDEX：获取接口索引
 *
 * <linux/if_packet.h>
 *   → struct sockaddr_ll：链路层地址
 *
 * <net/ethernet.h>
 *   → ETH_ALEN (6)：MAC 地址长度
 *   → ETH_HLEN (14)：以太网头长度
 *
 * <arpa/inet.h>
 *   → ntohs()：网络字节序转主机序
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

/* ==================== 第2部分：宏定义 ==================== */
/*
 * DEFAULT_ETHERTYPE：默认以太网类型
 *
 * 选择 0x88B5 的原因（与 stage01 相同）：
 *   → IEEE 保留的实验协议（EtherType 0x8800~0x88B5 是实验用途）
 *   → 不会被标准协议栈处理（不会有 ARP/IPv4/IPv6 解析）
 *   → 包会直接被提交到 ndo_start_xmit
 *   → 不会被真实网卡发送出去（不会污染网络）
 */
#define DEFAULT_ETHERTYPE 0x88B5

/*
 * MAX_FRAME_SIZE：最大帧大小
 *
 * 以太网标准：
 *   → 最小帧：64 字节（目的MAC+源MAC+类型+数据+CRC）
 *   → 最大帧：1518 字节（MTU 1500 + ETH_HLEN 14 + CRC 4）
 *   → Jumbo Frame：9000 字节
 */
#define MAX_FRAME_SIZE 1514

/* ==================== 第3部分：usage 说明 ==================== */
/*
 * usage - 打印命令行用法
 *
 * 【参数说明】
 *   <ifname>      → 目标接口名（如 nds2）
 *   [payload]     → 可选负载数据（默认: stage02-default-payload）
 *   [ethertype]   → 可选 ETHERTYPE（默认: 0x88B5）
 *
 * 【为什么 ETHERTYPE 默认是 0x88B5？】
 *   → 静默协议，不会触发标准协议处理
 *   → 适合测试环回
 *
 * 【示例】
 *   ./send_stage02_frame nds2 hello           → 发送 hello，ETHERTYPE=0x88B5
 *   ./send_stage02_frame nds2 hello 0x0800    → 发送 hello，ETHERTYPE=0x0800 (IPv4)
 *   ./send_stage02_frame nds2 hello 0x0806    → 发送 hello，ETHERTYPE=0x0806 (ARP)
 */
static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s <ifname> [payload] [ethertype]\n"
		"  <ifname>     : 目标接口名（如 nds2）\n"
		"  [payload]    : 可选负载数据（默认: stage02-default-payload）\n"
		"  [ethertype]  : 可选 ETHERTYPE（默认: 0x88B5）\n"
		"\n"
		"Example: %s nds2 hello_stage02 0x88B5\n",
		prog, prog);
}

/* ==================== 第4部分：main 函数 ==================== */
int main(int argc, char **argv)
{
	int fd = -1;
	struct ifreq ifr;
	struct sockaddr_ll addr;
	unsigned char frame[MAX_FRAME_SIZE];
	const char *ifname;
	const char *payload = "stage02-default-payload";
	unsigned int ethertype = DEFAULT_ETHERTYPE;
	size_t payload_len;
	size_t frame_len;

	/*
	 * 【参数解析】
	 *
	 * argv[0] = 程序名
	 * argv[1] = 接口名（必须）
	 * argv[2] = payload（可选）
	 * argv[3] = ethertype（可选）
	 */
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	ifname = argv[1];
	if (argc >= 3)
		payload = argv[2];
	if (argc >= 4)
		ethertype = (unsigned int)strtoul(argv[3], NULL, 0);

	/*
	 * 【payload 长度检查】
	 *
	 * 以太网帧格式：
	 *   [0-5]   目的 MAC（6字节）
	 *   [6-11]  源 MAC（6字节）
	 *   [12-13] ETHERTYPE（2字节）
	 *   [14+]   payload（可变）
	 *
	 * MAX_FRAME_SIZE = 1514 = ETH_HLEN(14) + 1500(最大MTU)
	 * 所以 payload 最大长度 = 1514 - 14 = 1500
	 */
	payload_len = strlen(payload);
	if (payload_len > MAX_FRAME_SIZE - ETH_HLEN) {
		fprintf(stderr, "payload too large (max %d bytes)\n",
			MAX_FRAME_SIZE - ETH_HLEN);
		return 1;
	}

	/*
	 * 【创建 AF_PACKET/SOCK_RAW 原始套接字】
	 *
	 * socket() 参数：
	 *   domain   = AF_PACKET   ：链路层套接字
	 *   type     = SOCK_RAW     ：接收/发送原始帧（包含 ETH_HEADER）
	 *   protocol = htons(TYPE)  ：接受的 ETHERTYPE
	 *                             htons(ETH_P_ALL) = 接收所有协议
	 *                             htons(0x88B5) = 只接收静默协议
	 *
	 * 【SOCK_RAW vs SOCK_DGRAM】
	 *   → SOCK_RAW：应用层直接处理以太网头
	 *   → SOCK_DGRAM：内核帮处理以太网头
	 *   → 我们的目标是构造完整帧，所以用 SOCK_RAW
	 */
	fd = socket(AF_PACKET, SOCK_RAW, htons((unsigned short)ethertype));
	if (fd < 0) {
		perror("socket(AF_PACKET, SOCK_RAW)");
		return 1;
	}

	/*
	 * 【获取接口索引（ifindex）】
	 *
	 * ioctl SIOCGIFINDEX：
	 *   → 把接口名（如 "nds2"）转换为内核内部索引
	 *   → 后续 sendto() 需要用 ifindex 指定发送接口
	 */
	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
		perror("ioctl(SIOCGIFINDEX)");
		close(fd);
		return 1;
	}

	/*
	 * 【填充 sockaddr_ll 地址结构】
	 *
	 * sockaddr_ll 是 AF_PACKET 套接字的地址结构：
	 *   sll_family    = AF_PACKET      ：地址族
	 *   sll_ifindex   = ifr.ifr_ifindex：接口索引
	 *   sll_halen     = ETH_ALEN       ：MAC 地址长度（6）
	 *   sll_addr[8]   = 目标 MAC       ：全 ff（广播）
	 *
	 * 【为什么用广播？】
	 *   → stage02 驱动不做 MAC 过滤
	 *   → 广播确保帧一定能被接收
	 */
	memset(&addr, 0, sizeof(addr));
	addr.sll_family  = AF_PACKET;
	addr.sll_ifindex = ifr.ifr_ifindex;
	addr.sll_halen   = ETH_ALEN;
	memset(addr.sll_addr, 0xff, ETH_ALEN); /* 广播 */

	/*
	 * 【构造以太网帧】
	 *
	 * 帧格式：
	 *   [0-5]   目的 MAC  ：ff ff ff ff ff ff（广播）
	 *   [6-11]  源 MAC    ：12 12 12 12 12 12（合成）
	 *   [12-13] ETHERTYPE：用户指定
	 *   [14+]   payload  ：用户数据
	 *
	 * ETHERTYPE 的字节序：
	 *   → 帧里是网络字节序（大端）
	 *   → ethertype >> 8 得到高字节
	 *   → ethertype & 0xff 得到低字节
	 */
	memset(frame, 0, sizeof(frame));
	memset(frame, 0xff, ETH_ALEN);                    /* 目的 MAC：广播 */
	memset(frame + ETH_ALEN, 0x12, ETH_ALEN);          /* 源 MAC：合成 */
	frame[12] = (unsigned char)((ethertype >> 8) & 0xff); /* ETHERTYPE 高字节 */
	frame[13] = (unsigned char)(ethertype & 0xff);     /* ETHERTYPE 低字节 */
	memcpy(frame + ETH_HLEN, payload, payload_len);   /* 负载数据 */
	frame_len = ETH_HLEN + payload_len;               /* 总帧长度 */

	/*
	 * 【发送帧】
	 *
	 * sendto() 参数：
	 *   fd       ：socket 文件描述符
	 *   frame    ：要发送的帧缓冲区
	 *   frame_len：帧长度
	 *   flags    ：0（默认）
	 *   dest_addr：sockaddr_ll，指定目标接口和 MAC
	 *   addrlen  ：sizeof(sockaddr_ll)
	 *
	 * 【发送流程】
	 *   sendto() → 内核协议栈 → __dev_queue_xmit()
	 *   → 根据 ETHERTYPE 查找协议处理
	 *   → 0x88B5 没有注册 → 直接提交到 ndo_start_xmit
	 *   → 驱动收到帧
	 */
	if (sendto(fd, frame, frame_len, 0,
		   (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("sendto");
		close(fd);
		return 1;
	}

	printf("sent %zu bytes via %s (ifindex=%d, ETHERTYPE=0x%04x)\n",
	       frame_len, ifname, ifr.ifr_ifindex, ethertype & 0xffff);

	close(fd);
	return 0;
}

/*
 * ==================== 附录：帧格式详解 ====================
 *
 * 【以太网帧结构（IEEE 802.3）】
 *
 *   字段          偏移    长度    说明
 *   ─────────────────────────────────────────────────────
 *   目的 MAC       0       6      接收方物理地址
 *   源 MAC         6       6      发送方物理地址
 *   ETHERTYPE      12      2      上层协议类型
 *                                     0x0800 = IPv4
 *                                     0x0806 = ARP
 *                                     0x86DD = IPv6
 *                                     0x88B5 = 静默协议
 *   Payload        14      46~1500 数据负载
 *   CRC            最后     4      帧校验序列（内核自动计算）
 *
 * 【常见 ETHERTYPE 对照表】
 *   0x0800 = IPv4
 *   0x0806 = ARP
 *   0x86DD = IPv6
 *   0x88B5 = 静默协议（实验用途）
 *   0x88B6 = OUI Extended
 *   0x88B7 = Reserved
 *
 * 【与 stage01 的区别】
 *   stage01 只支持 0x88B5，stage02 支持任意 ETHERTYPE
 *   自定义 ETHERTYPE 可以测试不同的协议解析路径
 */
