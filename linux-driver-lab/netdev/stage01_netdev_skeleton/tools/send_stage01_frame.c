// SPDX-License-Identifier: GPL-2.0
/*
 * send_stage01_frame.c - 用户态原始套接字发包工具
 *
 * ==================== 文件概述 ====================
 *
 * 本工具用于 stage01 smoke 测试：向指定接口发送原始以太网帧，
 * 触发 netdev_stage01.c 的 ndo_start_xmit 回调。
 *
 * 【学习焦点】
 *   1. AF_PACKET / SOCK_RAW：链路层原始套接字的工作原理
 *   2. sockaddr_ll：链路层地址结构
 *   3. ETHERTYPE 选择：为什么用 0x88B5（IEEE 静默协议）
 *   4. SIOCGIFINDEX：如何把接口名转换为 ifindex
 *   5. 帧格式：以太网头的每一字节的含义
 *
 * 【为什么需要这个工具？】
 *   → ip link 只能管理接口状态（up/down/addr）
 *   → 不能往指定接口发送自定义数据帧
 *   → AF_PACKET/SOCK_RAW 允许我们构造任意 ETHERTYPE 的帧
 *
 * 【与 ndo_start_xmit 的关系】
 *   → sendto() 发送的帧穿过内核网络栈
 *   → __dev_queue_xmit() 根据 ETHERTYPE 找到对应协议处理
 *   → 最终调用驱动的 ndo_start_xmit()
 *   → 我们的 ETHERTYPE 0x88B5 不会被标准协议处理，直接提交到 xmit
 *
 * ==================== 代码结构 ====================
 *
 *  1. 头部注释与 include（第1~40行）
 *  2. 宏定义与默认值（第42~52行）
 *  3. usage 说明（第54~64行）
 *  4. main 函数（第66~91行）
 *
 * 【为什么没有封装成函数？】
 *   → 工具代码保持最小化，逻辑简单直接
 *   → 便于教学演示：所有步骤都在 main 里一目了然
 */

/* ==================== 第1部分：头文件 ==================== */
#define _GNU_SOURCE           /*needed for struct ifreq on some glibc systems */
#include <arpa/inet.h>          /* ntohs()：网络字节序转主机序 */
#include <errno.h>             /* errno 全局变量 */
#include <net/ethernet.h>      /* ETH_ALEN (6)：MAC 地址长度 */
#include <stdbool.h>            /* bool 类型 */
#include <stdio.h>             /* fprintf / printf */
#include <stdlib.h>            /* malloc / free */
#include <string.h>            /* memset / memcpy */
#include <sys/socket.h>        /* socket() / sendto() / close() */
#include <sys/ioctl.h>         /* ioctl()：设备控制 */
#include <net/if.h>            /* struct ifreq：ioctl 接口名 */
#include <unistd.h>            /* close() */
#include <linux/if_packet.h>   /* struct sockaddr_ll：链路层地址 */

/*
 * 【头文件选择说明】
 *
 * linux/if_packet.h：
 *   → AF_PACKET 套接字的地址结构体 sockaddr_ll
 *   → PACKET_ADD_MEMBERSHIP 等宏
 *
 * linux/if.h：
 *   → struct ifreq：用于 ioctl 获取接口信息
 *   → SIOCGIFINDEX：获取接口索引
 *
 * net/ethernet.h：
 *   → ETH_ALEN (6)：以太网 MAC 地址长度
 *   → ETH_HLEN (14)：以太网头长度
 *
 * arpa/inet.h：
 *   → ntohs()：16位网络字节序转主机序
 *   → 用于 ETHERTYPE 显示
 */

/* ==================== 第2部分：宏与默认值 ==================== */
/*
 * DEFAULT_ETHERTYPE：默认以太网类型
 *
 * 选择 0x88B5 的原因：
 *   → IEEE 保留的实验协议（EtherType 0x8800~0x88B5 是实验用途）
 *   → 不会被标准协议栈处理（不会有 ARP/IPv4/IPv6 解析）
 *   → 包会直接被提交到 ndo_start_xmit
 *   → 不会被真实网卡发送出去（不会污染网络）
 *
 * 其他可用选择：
 *   → 0x88B5 (LAC) / 0x88B6 (OUI Extended) / 0x88B7 (Reserved)
 *   → 0x88B5 是最常用的静默测试协议
 */
#define DEFAULT_ETHERTYPE 0x88B5

/*
 * MAX_FRAME_SIZE：最大帧大小
 *
 * 以太网标准：
 *   → 最小帧：64 字节（目的MAC+源MAC+类型+数据+CRC）
 *   → 最大帧：1518 字节（MTU 1500 + ETH_HLEN 14 + CRC 4）
 *   → Jumbo Frame：9000 字节（但 stage01 用标准帧即可）
 */
#define MAX_FRAME_SIZE 1514

/* ==================== 第3部分：usage 说明 ==================== */
/*
 * usage：打印命令行用法
 *
 * 【为什么不返回错误码而是 exit(1)？】
 *   → usage 是在参数错误时调用的
 *   → 用户看 Usage 是调试行为，不是程序正常流程
 *   → exit(1) 表示"错误退出"
 */
static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <ifname> [payload]\n"
            "  <ifname>  : 目标接口名（如 nds0）\n"
            "  [payload] : 可选负载数据（默认: stage01-default-payload）\n"
            "\n"
            "Example: %s nds0 hello_stage01\n"
            "\n"
            "ETHERTYPE : 0x%04x（IEEE 静默协议，不触发标准解析）\n",
            prog, prog, DEFAULT_ETHERTYPE);
}

/* ==================== 第4部分：main 函数 ==================== */
int main(int argc, char **argv)
{
    int fd = -1;
    struct ifreq ifr;
    struct sockaddr_ll addr;
    unsigned char frame[MAX_FRAME_SIZE];
    const char *ifname;
    const char *payload = "stage01-default-payload";
    size_t payload_len;
    size_t frame_len;

    /*
     * 【参数解析】
     *
     * argc / argv 语义：
     *   argv[0] = 程序名（send_stage01_frame）
     *   argv[1] = 接口名（必须）
     *   argv[2] = payload（可选，默认 "stage01-default-payload"）
     *
     * 为什么 payload 默认值是字符串而不是空？
     *   → 空 payload 的帧仍然合法，但显示效果差
     *   → "stage01-default-payload" 便于识别这个帧来自我们的工具
     */
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    ifname = argv[1];
    if (argc >= 3)
        payload = argv[2];

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
     *                             htons(0x88B5) = 只接收我们的静默协议
     *
     * 【为什么 protocol 用 0x88B5？】
     *   → 这样 socket 只接收 ETHERTYPE=0x88B5 的帧
     *   → 避免被其他协议干扰
     *   → 发送时可以用任意 ETHERTYPE
     *
     * 【SOCK_RAW vs SOCK_DGRAM】
     *   → SOCK_RAW：应用层直接处理以太网头（我们需要这个）
     *   → SOCK_DGRAM：内核帮我们处理以太网头（不需要手动填充 dst/src MAC）
     *   → 我们的目标是构造完整帧，所以用 SOCK_RAW
     */
    fd = socket(AF_PACKET, SOCK_RAW, htons(DEFAULT_ETHERTYPE));
    if (fd < 0) {
        perror("socket(AF_PACKET, SOCK_RAW)");
        return 1;
    }

    /*
     * 【获取接口索引（ifindex）】
     *
     * ioctl SIOCGIFINDEX：
     *   → 把接口名（如 "nds0"）转换为内核内部索引（如 2）
     *   → ifindex 是内核识别网络设备的唯一编号
     *   → 后续 sendto() 需要用 ifindex 指定发送接口
     *
     * struct ifreq：
     *   → ifr_ifindex：接口索引
     *   → ifr_name：接口名
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
     *   sll_halen     = ETH_ALEN       ：MAC 地址长度
     *   sll_addr[8]   = 目标 MAC       ：我们用全 ff（广播）
     *
     * 【为什么用广播（全 ff）而不是单播？】
     *   → stage01 驱动不做 MAC 过滤
     *   → 广播确保帧一定能被接收
     *   → 真实网卡会检查目的 MAC 是否匹配
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
     *   [12-13] ETHERTYPE：88 B5（静默协议）
     *   [14+]   payload  ：用户数据
     *
     * 为什么源 MAC 用固定值 0x12？
     *   → 这是一个教学工具，不需要真实源 MAC
     *   → 用全零也可以，驱动不在乎
     *
     * ETHERTYPE 0x88B5 的字节序：
     *   → 帧里是网络字节序（大端）：[12]=0x88, [13]=0xB5
     *   → htons(0x88B5) = 0xB588（在小端机器上）
     *   → 所以 frame[12] = (0x88B5 >> 8) & 0xff = 0x88
     *   → frame[13] = 0x88B5 & 0xff = 0xB5
     */
    memset(frame, 0, sizeof(frame));
    memset(frame, 0xff, ETH_ALEN);                    /* 目的 MAC：广播 */
    memset(frame + ETH_ALEN, 0x12, ETH_ALEN);          /* 源 MAC：合成 */
    frame[12] = (DEFAULT_ETHERTYPE >> 8) & 0xff;      /* ETHERTYPE 高字节 */
    frame[13] = DEFAULT_ETHERTYPE & 0xff;              /* ETHERTYPE 低字节 */
    memcpy(frame + ETH_HLEN, payload, payload_len);   /* 负载数据 */
    frame_len = ETH_HLEN + payload_len;               /* 总帧长度 */

    /*
     * 【发送帧】
     *
     * sendto() 参数：
     *   fd      ：socket 文件描述符
     *   frame   ：要发送的帧缓冲区
     *   frame_len：帧长度（ETH_HLEN + payload_len）
     *   flags   ：0（默认）
     *   dest_addr： sockaddr_ll，指定目标接口和 MAC
     *   addrlen ：sizeof(sockaddr_ll)
     *
     * 【发送流程】
     *   sendto() → 内核协议栈 → 根据 ETHERTYPE 查找协议处理
     *   → 0x88B5 没有注册协议处理 → 直接提交到 ndo_start_xmit
     *   → 驱动收到帧，调用 stage01_start_xmit()
     *
     * 【返回值】
     *   成功：返回发送的字节数（frame_len）
     *   失败：返回 -1，设置 errno
     */
    if (sendto(fd, frame, frame_len, 0,
               (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("sendto");
        close(fd);
        return 1;
    }

    printf("sent %zu bytes via %s (ifindex=%d, ETHERTYPE=0x%04x)\n",
           frame_len, ifname, ifr.ifr_ifindex, DEFAULT_ETHERTYPE);

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
 *   目的 MAC       0       6      接收方物理地址（ff ff ff ff ff ff = 广播）
 *   源 MAC         6       6      发送方物理地址（12 12 12 12 12 12 = 合成）
 *   ETHERTYPE      12      2      上层协议类型
 *                                     0x0800 = IPv4
 *                                     0x0806 = ARP
 *                                     0x86DD = IPv6
 *                                     0x88B5 = 静默协议（实验用途）
 *   Payload        14      46~1500 数据负载
 *   CRC            最后    4      帧校验序列（内核自动计算）
 *
 * 【我们的帧示例】
 *   发送 "hello"（5字节）时：
 *     ff ff ff ff ff ff  目的 MAC（广播）
 *     12 12 12 12 12 12  源 MAC（合成）
 *     88 b5              ETHERTYPE（静默协议）
 *     68 65 6c 6c 6f      payload（"hello" ASCII）
 *     总帧长 = 14 + 5 = 19 字节
 *
 * 【为什么 ETHERTYPE 用 0x88B5？】
 *   → 这是 IEEE 保留的实验协议范围（0x8800~0x88B5）
 *   → 不会触发任何标准协议解析
 *   → 帧会直接被提交到 ndo_start_xmit
 *   → 适合教学验证
 *
 * 【AF_PACKET/SOCK_RAW 的发送流程】
 *
 *   sendto(socket_fd, frame, frame_len, 0, addr, addrlen)
 *     │
 *     ▼
 *   sock->ops->sendmsg()   ← 协议特定发送函数
 *     │
 *     ▼
 *   dev_queue_xmit(skb)   ← 放入设备 TX 队列
 *     │
 *     ▼
 *   __dev_queue_xmit()    ← 设备无关的发送入口
 *     │
 *     ▼
 *   validate_xmit_skb()    ← 检查 skb
 *     │
 *     ▼
 *   netdev_start_xmit()    ← 调用驱动的 ndo_start_xmit
 *     │
 *     ▼
 *   stage01_start_xmit(skb, ndev)  ← 驱动收到帧
 *     ├→ stage01_update_tx_stats()
 *     └→ dev_consume_skb_any(skb)  ← stage01 教学策略：直接消费
 */
