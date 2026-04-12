// SPDX-License-Identifier: GPL-2.0
/*
 * send_stage03_frame.c - 用户态原始套接字发包工具（stage03 专用）
 *
 * ==================== 文件概述 ====================
 *
 * stage03 的发送工具比 stage02 多支持：
 *   1. burst 发送（count 参数）
 *   2. 可配置帧间隔（interval_us 参数）
 *   → 这样才能测试 NAPI 的 pending queue 和 budget 语义
 *
 * 【学习焦点】
 *   1. AF_PACKET / SOCK_RAW：链路层原始套接字
 *   2. sockaddr_ll：链路层地址结构
 *   3. burst 发送与帧间隔控制
 *   4. ETHERTYPE 0x88B6（stage03 专用静默协议）
 *
 * ==================== 与 stage02 的区别 ====================
 *
 * stage02：sendto() 只发一帧
 * stage03：支持 count 次 burst 发送，支持 us 级别间隔控制
 *
 * 目的：测试 NAPI 在高频/批量场景下的行为
 *   → burst 发送能触发 pending queue 积压
 *   → 减小 interval_us 能测试 poll 的 budget 耗尽
 *
 * ==================== 代码结构 ====================
 *
 *  1. 头部注释与 include（第1~50行）
 *  2. 宏定义（第52~62行）
 *  3. usage 说明（第64~88行）
 *  4. main 函数（第90~180行）
 */

/* ==================== 第1部分：头文件 ==================== */
/*
 * 【头文件选择说明】
 *
 * _GNU_SOURCE：
 *   → glibc 系统需要此宏
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
 *
 * <unistd.h>
 *   → usleep()：微秒级延迟（用于帧间隔控制）
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
 * stage03 使用 0x88B6（stage02 用 0x88B5）
 * 两个都是 IEEE 保留的实验协议，不会被标准协议栈处理
 */
#define DEFAULT_ETHERTYPE 0x88B6

/*
 * MAX_FRAME_SIZE：最大帧大小
 *
 * 以太网标准：
 *   → 最小帧：64 字节
 *   → 最大帧：1518 字节（MTU 1500 + ETH_HLEN 14 + CRC 4）
 */
#define MAX_FRAME_SIZE 1514

/* ==================== 第3部分：usage 说明 ==================== */
/*
 * usage - 打印命令行用法
 *
 * 【参数说明】
 *   <ifname>       → 目标接口名（如 nds3）
 *   [payload]      → 可选负载数据（默认: stage03-default-payload）
 *   [ethertype]   → 可选 ETHERTYPE（默认: 0x88B6）
 *   [count]       → 可选 burst 数量（默认: 1）
 *   [interval_us] → 两帧之间的 us 间隔（默认: 0）
 *
 * 【为什么要支持 burst？】
 *   → 测试 NAPI 的 pending queue 积压场景
 *   → count=32 表示一次发送 32 帧
 *   → interval_us=0 表示尽可能快地发送
 *
 * 【示例】
 *   ./send_stage03_frame nds3 hello             → 发 1 帧
 *   ./send_stage03_frame nds3 hello 0x88B6 32 0  → 发 32 帧，无间隔
 *   ./send_stage03_frame nds3 hello 0x88B6 10 1000 → 发 10 帧，每帧间隔 1000us
 */
static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s <ifname> [payload] [ethertype] [count] [interval_us]\n"
		"  <ifname>      : 目标接口名（如 nds3）\n"
		"  [payload]     : 可选负载数据（默认: stage03-default-payload）\n"
		"  [ethertype]   : 可选 ETHERTYPE（默认: 0x88B6）\n"
		"  [count]       : 可选 burst 数量（默认: 1）\n"
		"  [interval_us] : 两帧之间的 us 间隔（默认: 0）\n"
		"\n"
		"Example: %s nds3 hello 0x88B6 32 0\n",
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
	const char *payload = "stage03-default-payload";
	unsigned int ethertype = DEFAULT_ETHERTYPE;
	unsigned int count = 1;
	unsigned int interval_us = 0;
	size_t payload_len;
	size_t frame_len;
	unsigned int i;

	/*
	 * 【参数解析】
	 *
	 * argv[0] = 程序名
	 * argv[1] = 接口名（必须）
	 * argv[2] = payload（可选）
	 * argv[3] = ethertype（可选）
	 * argv[4] = count（可选，burst 数量）
	 * argv[5] = interval_us（可选，帧间隔）
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
	if (argc >= 5)
		count = (unsigned int)strtoul(argv[4], NULL, 0);
	if (argc >= 6)
		interval_us = (unsigned int)strtoul(argv[5], NULL, 0);
	if (count == 0)
		count = 1;

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
	 */
	payload_len = strlen(payload);
	if (payload_len > MAX_FRAME_SIZE - ETH_HLEN) {
		fprintf(stderr, "payload too large (max %d bytes)\n", MAX_FRAME_SIZE - ETH_HLEN);
		return 1;
	}

	/*
	 * 【创建 AF_PACKET/SOCK_RAW 原始套接字】
	 *
	 * socket() 参数：
	 *   domain   = AF_PACKET    ：链路层套接字
	 *   type     = SOCK_RAW      ：原始帧（包含 ETH_HEADER）
	 *   protocol = htons(TYPE)   ：ETHERTYPE 过滤
	 *
	 * SOCK_RAW vs SOCK_DGRAM：
	 *   → SOCK_RAW：应用层直接处理以太网头
	 *   → SOCK_DGRAM：内核帮处理以太网头
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
	 *   → 把接口名（如 "nds3"）转换为内核内部索引
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
	 * sockaddr_ll：
	 *   sll_family    = AF_PACKET      ：地址族
	 *   sll_ifindex  = ifr.ifr_ifindex：接口索引
	 *   sll_halen    = ETH_ALEN       ：MAC 地址长度（6）
	 *   sll_addr[8] = 目标 MAC       ：全 ff（广播）
	 */
	memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = ifr.ifr_ifindex;
	addr.sll_halen = ETH_ALEN;
	memset(addr.sll_addr, 0xff, ETH_ALEN);

	/*
	 * 【构造以太网帧】
	 *
	 * 帧格式：
	 *   [0-5]   目的 MAC  ：ff ff ff ff ff ff（广播）
	 *   [6-11]  源 MAC    ：23 23 23 23 23 23（合成，与 stage02 不同）
	 *   [12-13] ETHERTYPE：用户指定
	 *   [14+]   payload  ：用户数据
	 */
	memset(frame, 0, sizeof(frame));
	memset(frame, 0xff, ETH_ALEN);                    /* 目的 MAC：广播 */
	memset(frame + ETH_ALEN, 0x23, ETH_ALEN);          /* 源 MAC：合成 */
	frame[12] = (unsigned char)((ethertype >> 8) & 0xff); /* ETHERTYPE 高字节 */
	frame[13] = (unsigned char)(ethertype & 0xff);     /* ETHERTYPE 低字节 */
	memcpy(frame + ETH_HLEN, payload, payload_len);   /* 负载数据 */
	frame_len = ETH_HLEN + payload_len;               /* 总帧长度 */

	/*
	 * 【burst 发送循环】
	 *
	 * count 次循环，每次 sendto() 发一帧
	 * interval_us > 0 时，两帧之间 usleep() 延迟
	 *
	 * 【为什么需要这个？】
	 *   → 测试 NAPI 的 pending queue 积压
	 *   → 快速 burst 发送可能让 pending queue 来不及 drain
	 *   → 观察 napi_budget_exhaust_count / pending_peak
	 */
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

	printf("[send_stage03_frame] sent %u frame(s) on %s ethertype=0x%04x payload=\"%s\"\n",
	       count, ifname, ethertype & 0xffff, payload);
	close(fd);
	return 0;
}

/*
 * ==================== 附录：stage03 测试场景 ====================
 *
 * 【场景1：基本环回测试】
 *   ./send_stage03_frame nds3 hello 0x88B6 1 0
 *   → 验证 direct/napi 模式基本功能
 *
 * 【场景2：NAPI pending queue 积压测试】
 *   ./send_stage03_frame nds3 hello 0x88B6 32 0
 *   → 快速 burst 发送 32 帧
 *   → 观察 pending_peak 是否 > 1
 *
 * 【场景3：budget exhaustion 测试】
 *   ./send_stage03_frame nds3 hello 0x88B6 64 0
 *   + napi_weight=4
 *   → 每次 poll 只处理 4 帧
 *   → 64 帧需要 16 次 poll
 *   → 观察 napi_budget_exhaust_count 是否增加
 *
 * 【场景4：低频发送测试】
 *   ./send_stage03_frame nds3 hello 0x88B6 10 10000
 *   → 每帧间隔 10ms
 *   → 观察每次只处理 1 帧，pending_peak=1
 *
 * ==================== 附录：ETHERTYPE 0x88B5 vs 0x88B6 ====================
 *
 * 0x88B5：stage02 使用
 * 0x88B6：stage03 使用
 *
 * 两者都是 IEEE 保留的实验协议，不会被标准协议栈处理。
 * 选不同的值是为了避免 stage02 和 stage03 互相干扰。
 */
