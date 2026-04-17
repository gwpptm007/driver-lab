// SPDX-License-Identifier: GPL-2.0
/*
 * send_stage08_frame.c — 发送自定义以太网帧到指定接口
 *
 * 【学习要点】
 *
 * 1. AF_PACKET / SOCK_RAW 的作用
 *    - 允许用户态直接构造和发送任意以太类型（ethertype）的原始帧
 *    - 不经过协议栈，直接送到网卡
 *
 * 2. 帧格式
 *    - 前 14 字节是以太网头（ETH_HLEN = 14）
 *      - 目标 MAC（6字节）+ 源 MAC（6字节）+ ethertype（2字节）
 *    - ethertype = 0x88B8 是本实验自定义类型（非标准）
 *
 * 3. 关键 API
 *    - socket(AF_PACKET, SOCK_RAW, htons(ethertype))：创建原始套接字
 *    - setsockopt(PACKET_IGNORE_OUTGOING)：忽略本机发出的同协议帧（避免回环）
 *    - sendto()：发送原始帧到指定接口
 *
 * 4. 使用场景
 *    用于向 netdev_stage08 驱动发送测试帧，触发 ndo_start_xmit
 *
 * 【用法】
 *   ./send_stage08_frame <ifname> [payload] [ethertype] [count] [interval_us]
 *
 * 【例子】
 *   ./send_stage08_frame nds8 "hello" 0x88B8 32 0
 *     -> 在 nds8 上发送 32 帧，每帧 payload="hello"，ethertype=0x88B8
 */

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

#define DEFAULT_ETHERTYPE 0x88B8
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
    const char *payload = "stage08-default-payload";
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

    /*
     * 【学习】AF_PACKET + SOCK_RAW
     * - AF_PACKET：链路层套接字
     * - SOCK_RAW：原始模式，不添加以太网头
     * - ETH_P_ALL：接收所有协议类型的帧
     *
     * 注意：这里用 ETH_P_ALL 而非具体的 ethertype，
     * 是因为发送时我们手动构造以太网头
     */
    fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0) {
        perror("socket(AF_PACKET)");
        return 1;
    }

    /*
     * 【学习】SIOCGIFINDEX ioctl
     * 用于获取网卡接口索引（ifindex）
     * bind() 和 sendto() 需要这个索引来指定从哪个接口发送
     */
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl(SIOCGIFINDEX)");
        close(fd);
        return 1;
    }

    /*
     * 【学习】 sockaddr_ll 结构
     * - sll_family：必须为 AF_PACKET
     * - sll_ifindex：接口索引
     * - sll_halen：MAC 地址长度（ETH_ALEN = 6）
     * - sll_addr：目标 MAC 地址
     */
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = ifr.ifr_ifindex;
    addr.sll_halen = ETH_ALEN;
    memset(addr.sll_addr, 0xff, ETH_ALEN);  // 广播地址

    /*
     * 【学习】构造以太网帧
     * - 前 ETH_ALEN 字节：目标 MAC（全 0xff = 广播）
     * - 后 ETH_ALEN 字节：源 MAC（这里用 0x27 填充）
     * - 2 字节：ethertype（大端序）
     * - 后面是 payload
     */
    memset(frame, 0, sizeof(frame));
    memset(frame, 0xff, ETH_ALEN);                      // 目标 MAC
    memset(frame + ETH_ALEN, 0x27, ETH_ALEN);            // 源 MAC（任意值）
    frame[12] = (unsigned char)((ethertype >> 8) & 0xff);  // ethertype 高字节
    frame[13] = (unsigned char)(ethertype & 0xff);       // ethertype 低字节
    memcpy(frame + ETH_HLEN, payload, payload_len);
    frame_len = ETH_HLEN + payload_len;

    /*
     * 【学习】sendto() 发送原始帧
     * - frame：待发送的数据（包含以太网头）
     * - frame_len：总长度
     * - flags：默认 0
     * - sockaddr：目标地址（包含接口索引）
     * - addrlen：地址长度
     *
     * 注意：帧会从指定的 ifindex 接口发出，
     * 即使 sll_addr 是广播地址，网卡也只接收，
     * 实际行为由 ethertype 和网卡驱动决定
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

    printf("[send_stage08_frame] sent %u frame(s) on %s ethertype=0x%04x payload=\"%s\"\n",
           count, ifname, ethertype & 0xffff, payload);
    close(fd);
    return 0;
}