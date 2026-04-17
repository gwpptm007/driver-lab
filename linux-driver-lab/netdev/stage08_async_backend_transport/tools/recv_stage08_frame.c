// SPDX-License-Identifier: GPL-2.0
/*
 * recv_stage08_frame.c — 接收指定接口上的原始以太网帧
 *
 * 【学习要点】
 *
 * 1. AF_PACKET / SOCK_RAW 接收模式
 *    - 绑定到特定 ethertype（0x88B8）
 *    - 只接收匹配该 ethertype 的帧
 *    - 帧包含完整的以太网头
 *
 * 2. recvfrom() 与 recv() 的区别
 *    - recvfrom() 获取发送者地址（ sockaddr_ll）
 *    - recv() 只返回数据
 *    - 这里用 recvfrom() 是为了获取包来源信息
 *
 * 3. PACKET_IGNORE_OUTGOING 套接字选项
 *    - 避免接收到本机发出的帧（回环）
 *    - 重要：发送和接收用同一个 ethertype 时需要设置
 *
 * 4. SO_RCVTIMEO 接收超时
 *    - 避免 recvfrom() 永久阻塞
 *    - 超时后返回 -1，errno = EAGAIN/EWOULDBLOCK
 *
 * 5. 帧解析
 *    - ETH_HLEN (14) 字节是以太网头
 *    - 前 6 字节：目标 MAC
 *    - 中间 6 字节：源 MAC
 *    - 后 2 字节：ethertype（大端序）
 *    - 剩余：payload
 *
 * 【用法】
 *   ./recv_stage08_frame <ifname> [ethertype] [max_frames] [timeout_sec]
 *
 * 【例子】
 *   ./recv_stage08_frame nds8 0x88B8 32 5
 *     -> 在 nds8 上最多接收 32 帧，超时 5 秒
 */

#ifndef PACKET_IGNORE_OUTGOING
#define PACKET_IGNORE_OUTGOING 23
#endif

#define DEFAULT_ETHERTYPE 0x88B8
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

    /*
     * 【学习】创建 AF_PACKET 原始套接字
     * - ethertype 指定了要接收的帧类型
     * - htons()：主机字节序转网络字节序（大端）
     *
     * 这里 ethertype = 0x88B8 是自定义值，
     * 表示只接收本实验驱动发出的帧
     */
    fd = socket(AF_PACKET, SOCK_RAW, htons((unsigned short)ethertype));
    if (fd < 0) {
        perror("socket(AF_PACKET)");
        return 1;
    }

    /*
     * 【学习】PACKET_IGNORE_OUTGOING
     * - 设置后，本机发出的同协议帧不会触发接收
     * - 避免 send + recv 在同一进程中产生回环
     */
    (void)setsockopt(fd, SOL_PACKET, PACKET_IGNORE_OUTGOING, &one, sizeof(one));

    /*
     * 【学习】SO_RCVTIMEO
     * - 设置 recvfrom() 超时时间
     * - tv.tv_sec：秒
     * - tv.tv_usec：微秒
     * - 超时后返回 -1，errno = EAGAIN
     */
    tv.tv_sec = (long)timeout_sec;
    tv.tv_usec = 0;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /*
     * 【学习】获取接口索引
     * SIOCGIFINDEX ioctl 获取 ifindex，
     * 后面 bind() 需要用到
     */
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl(SIOCGIFINDEX)");
        close(fd);
        return 1;
    }

    /*
     * 【学习】bind() 绑定到指定接口
     * - sockaddr_ll 结构指定了接口和协议
     * - 绑定后，只有从该接口收到的匹配 ethertype 的帧才会到达
     * - 注意：bind() 的 sll_protocol 会覆盖 socket() 创建时的协议
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
     * 【学习】recvfrom() 接收帧
     * - 返回值：接收到的字节数
     * - rx_addr：发送者的链路层地址（ sockaddr_ll）
     * - rx_addr_len：地址长度（会被设置）
     *
     * 【学习】以太网帧解析
     * - frame[0..5]：目标 MAC
     * - frame[6..11]：源 MAC
     * - frame[12..13]：ethertype（大端序）
     * - frame[14..]：payload
     *
     * ETH_HLEN = 14，是以太网头的固定长度
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
        printf("[recv_stage08_frame] #%u len=%zd ifindex=%d protocol=0x%04x payload=\"%s\"\n",
               received + 1, n, rx_addr.sll_ifindex,
               ntohs(rx_addr.sll_protocol),
               (n > ETH_HLEN) ? (char *)(frame + ETH_HLEN) : "");
        received++;
    }

    printf("[recv_stage08_frame] received %u frame(s) on %s ethertype=0x%04x\n",
           received, ifname, ethertype & 0xffff);
    close(fd);
    return 0;
}