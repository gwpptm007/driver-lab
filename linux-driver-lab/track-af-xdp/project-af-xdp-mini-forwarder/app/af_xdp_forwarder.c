// SPDX-License-Identifier: GPL-2.0
/*
 * af_xdp_forwarder.c — AF_XDP 用户态转发器
 *
 * 在 lab-af-xdp-socket-rings 基础上，增加用户态转发策略：
 *
 *   drop 模式：收到包后直接丢弃（归还可以frame回FILL ring）
 *   reflect 模式：收到包后从 TX ring 发回（loopback/自测）
 *
 * 整体数据路径：
 *   [NIC] → XDP_redirect → [AF_XDP RX ring] → 用户态处理(drop/reflect)
 *                                                        ↓
 *                                                   [AF_XDP TX ring] → [NIC]
 *
 * reflect 模式下，流程如下：
 *   1. 从 RX ring 取 descriptor
 *   2. 把包的地址+长度写入 TX ring（不改变内容，只换方向）
 *   3. sendto() 触发内核真正发送
 *   4. 从 COMPLETION ring 取完成通知，归还 frame 到 FILL ring
 *
 * 与 lab-af-xdp-socket-rings 的关键差异：
 *   - 多了一个 TX ring 的使用（reflect 模式需要发包）
 *   - 多了一个 sendto() 调用触发 TX
 *   - COMPLETION ring 的 descriptor 会返回已发送的地址，需要归还
 *
 * 参数：
 *   --ifname IFACE         网卡名（必填）
 *   --mode skb|native       XDP 模式（默认 skb）
 *   --forward drop|reflect  转发策略（默认 drop）
 *   --copy                  copy 模式（默认）
 *   --zero-copy             zero-copy 模式
 *   --duration SEC          运行时间（秒，默认 15）
 *   --interval SEC          统计打印间隔（秒，默认 1）
 */
#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <net/if.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

#include <linux/if_link.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <bpf/xsk.h>

/*============================================================
 * XDP / AF_XDP 常量兼容层
 *
 * XDP_USE_NEED_WAKEUP：通知内核在 ring 无数据时唤醒进程。
 * XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD：禁止 libbpf 自动加载 BPF program。
 *============================================================*/
#ifndef XDP_USE_NEED_WAKEUP
#define XDP_USE_NEED_WAKEUP (1 << 3)
#endif
#ifndef XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD
#define XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD (1 << 0)
#endif

/*============================================================
 * libbpf 版本兼容层
 *
 * Ubuntu 22.04 自带 libbpf 0.5.0，
 * bpf_xdp_attach / bpf_xdp_detach 是 libbpf 1.0+ 才有的 API。
 * 编译时报错：undefined reference to `bpf_xdp_attach'
 * 解决：用 bpf_set_link_xdp_fd() 替代，语义相同，兼容性更好。
 *============================================================*/
#if LIBBPF_VERSION < 100
#define bpf_xdp_attach(ifindex, prog_fd, xdp_flags, _) \
    bpf_set_link_xdp_fd(ifindex, prog_fd, xdp_flags)
#define bpf_xdp_detach(ifindex, xdp_flags, _) \
    bpf_set_link_xdp_fd(ifindex, -1, xdp_flags)
#endif

/*============================================================
 * UMEM / Ring 大小配置
 *
 * UMEM 8MB = 4096 个 frame，每个 2KB。
 * TX ring 用来发包（reflect 模式下才真正用到）。
 *============================================================*/
#define FRAME_SIZE 2048U          // 每个 frame 大小（字节）
#define NUM_FRAMES 4096U          // frame 总数
#define UMEM_SIZE ((uint64_t)FRAME_SIZE * NUM_FRAMES)  // 8MB

#define RX_RING_SIZE 2048U        // RX ring 容量
#define TX_RING_SIZE 2048U        // TX ring 容量（reflect 模式发包用）
#define FILL_RING_SIZE 2048U     // FILL ring 容量
#define COMP_RING_SIZE 2048U     // COMPLETION ring 容量
#define BATCH_SIZE 64U            // 每次 poll 处理的最大 descriptor 数

/*============================================================
 * fwd_mode — 用户态转发策略
 *
 * FWD_DROP    ：收到包后直接丢弃，归还 frame 到 FILL ring
 * FWD_REFLECT ：收到包后从 TX ring 发回（loopback 自测）
 *============================================================*/
enum fwd_mode { FWD_DROP = 0, FWD_REFLECT = 1 };

/*============================================================
 * app_cfg — 命令行参数配置
 *============================================================*/
struct app_cfg {
    const char *ifname;          // 网卡名（如 "ens192"）
    const char *obj_path;         // BPF object 文件路径
    int queue_id;                 // 绑定到的 RX 队列号
    int duration_sec;            // 程序运行时间（秒）
    int interval_sec;            // 统计打印间隔（秒）
    bool native_mode;            // true = native(drv)，false = generic(skb)
    bool zero_copy;             // true = zero-copy，false = copy
    bool use_poll;               // true = poll() 等待，false = busy loop
    enum fwd_mode mode;         // drop 或 reflect
};

/*============================================================
 * xsk_ctx — AF_XDP socket 上下文
 *
 * 封装一个 AF_XDP socket 及其所有关联资源：
 *   umem_area / umem  — UMEM 内存区域
 *   fq                — FILL ring（生产者侧，预填充可用 frame）
 *   cq                — COMPLETION ring（消费者侧，TX 完成通知）
 *   rx                — RX ring（消费者侧，接收内核传来的包描述符）
 *   tx                — TX ring（生产者侧，待发送的包描述符）
 *   xsk               — AF_XDP socket 句柄
 *============================================================*/
struct xsk_ctx {
    void *umem_area;             // UMEM 起始地址
    struct xsk_umem *umem;       // libbpf UMEM 句柄
    struct xsk_ring_prod fq;    // FILL ring（生产者）
    struct xsk_ring_cons cq;    // COMPLETION ring（消费者）
    struct xsk_ring_cons rx;    // RX ring（消费者）
    struct xsk_ring_prod tx;    // TX ring（生产者，reflect 模式发包用）
    struct xsk_socket *xsk;      // AF_XDP socket 句柄
};

/*============================================================
 * app_stats — 应用层统计
 *============================================================*/
struct app_stats {
    uint64_t rx_packets;         // 收到的数据包数
    uint64_t rx_bytes;           // 收到的总字节数
    uint64_t tx_packets;         // 发送的数据包数（reflect 模式）
    uint64_t tx_bytes;           // 发送的总字节数
    uint64_t dropped_packets;   // 丢弃的数据包数（drop 模式）
    uint64_t fill_recycled;     // 归还到 FILL ring 的 frame 数
    uint64_t tx_full_drops;     // TX ring 满时丢弃的包数
    uint64_t comp_packets;     // COMPLETION ring 中的完成通知数
    uint64_t rx_empty_polls;   // poll 到空数据的次数
};

// 全局停止标志，信号处理程序设置
static volatile sig_atomic_t stop;

// 信号处理函数
static void on_signal(int signo) { (void)signo; stop = 1; }

// 获取当前时间戳（秒）
static uint64_t now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec;
}

// 返回转发模式名称字符串
static const char *mode_name(enum fwd_mode mode)
{
    return mode == FWD_REFLECT ? "reflect" : "drop";
}

// 打印用法说明
static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s --ifname IFACE [options]\n"
            "  --queue N              RX queue id, default 0\n"
            "  --duration SEC         run duration, default 15\n"
            "  --interval SEC         stats interval, default 1\n"
            "  --mode skb|native       XDP attach mode, default skb\n"
            "  --copy                 request XDP copy mode, default\n"
            "  --zero-copy            request XDP zero-copy mode\n"
            "  --forward drop|reflect  userspace policy, default drop\n"
            "  --busy-poll            busy loop instead of poll(2)\n"
            "  --obj PATH             BPF object path\n", prog);
}

/*============================================================
 * parse_args — 解析命令行参数
 *============================================================*/
static int parse_args(int argc, char **argv, struct app_cfg *cfg)
{
    static const struct option opts[] = {
        {"ifname", required_argument, NULL, 'i'},
        {"queue", required_argument, NULL, 'q'},
        {"duration", required_argument, NULL, 'd'},
        {"interval", required_argument, NULL, 't'},
        {"mode", required_argument, NULL, 'm'},
        {"copy", no_argument, NULL, 'c'},
        {"zero-copy", no_argument, NULL, 'z'},
        {"forward", required_argument, NULL, 'f'},
        {"busy-poll", no_argument, NULL, 'b'},
        {"obj", required_argument, NULL, 'o'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0},
    };
    int c;
    while ((c = getopt_long(argc, argv, "i:q:d:t:m:czf:bo:h", opts, NULL)) != -1) {
        switch (c) {
        case 'i': cfg->ifname = optarg; break;
        case 'q': cfg->queue_id = atoi(optarg); break;
        case 'd': cfg->duration_sec = atoi(optarg); break;
        case 't': cfg->interval_sec = atoi(optarg); break;
        case 'm':
            if (strcmp(optarg, "native") == 0) cfg->native_mode = true;
            else if (strcmp(optarg, "skb") == 0) cfg->native_mode = false;
            else { fprintf(stderr, "invalid --mode %s\n", optarg); return -1; }
            break;
        case 'c': cfg->zero_copy = false; break;    // --copy
        case 'z': cfg->zero_copy = true; break;     // --zero-copy
        case 'f':
            if (strcmp(optarg, "drop") == 0) cfg->mode = FWD_DROP;
            else if (strcmp(optarg, "reflect") == 0) cfg->mode = FWD_REFLECT;
            else { fprintf(stderr, "invalid --forward %s\n", optarg); return -1; }
            break;
        case 'b': cfg->use_poll = false; break;     // --busy-poll
        case 'o': cfg->obj_path = optarg; break;
        case 'h': usage(argv[0]); exit(0);
        default: usage(argv[0]); return -1;
        }
    }
    if (!cfg->ifname || cfg->queue_id < 0 || cfg->duration_sec <= 0 || cfg->interval_sec <= 0) {
        usage(argv[0]);
        return -1;
    }
    return 0;
}

// 提高 RLIMIT_MEMLOCK 限制（UMEM 分配需要）
static int bump_memlock_rlimit(void)
{
    struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
    if (setrlimit(RLIMIT_MEMLOCK, &r)) { perror("setrlimit(RLIMIT_MEMLOCK)"); return -1; }
    return 0;
}

// 生成 XDP attach 标志
static int xdp_flags(const struct app_cfg *cfg)
{
    return XDP_FLAGS_UPDATE_IF_NOEXIST | (cfg->native_mode ? XDP_FLAGS_DRV_MODE : XDP_FLAGS_SKB_MODE);
}

// 生成 AF_XDP bind 标志
static int bind_flags(const struct app_cfg *cfg)
{
    int flags = XDP_USE_NEED_WAKEUP;
    flags |= cfg->zero_copy ? XDP_ZEROCOPY : XDP_COPY;
    return flags;
}

/*============================================================
 * create_umem — 创建 UMEM
 *
 * UMEM = 用户态的数据包缓冲区池，划分为固定大小的 frame。
 * 内核通过 descriptor（frame 地址）传递数据，而不是复制实际数据。
 *============================================================*/
static int create_umem(struct xsk_ctx *xsk)
{
    struct xsk_umem_config cfg = {
        .fill_size = FILL_RING_SIZE,
        .comp_size = COMP_RING_SIZE,
        .frame_size = FRAME_SIZE,
        .frame_headroom = 0,
        .flags = 0,
    };
    if (posix_memalign(&xsk->umem_area, getpagesize(), UMEM_SIZE)) { perror("posix_memalign"); return -1; }
    memset(xsk->umem_area, 0, UMEM_SIZE);
    if (xsk_umem__create(&xsk->umem, xsk->umem_area, UMEM_SIZE, &xsk->fq, &xsk->cq, &cfg)) {
        perror("xsk_umem__create");
        return -1;
    }
    return 0;
}

/*============================================================
 * create_socket — 创建 AF_XDP socket 并绑定到网卡 + 队列
 *============================================================*/
static int create_socket(struct xsk_ctx *ctx, const struct app_cfg *cfg)
{
    struct xsk_socket_config xsk_cfg = {
        .rx_size = RX_RING_SIZE,
        .tx_size = TX_RING_SIZE,
        .libbpf_flags = XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD,
        .xdp_flags = xdp_flags(cfg),
        .bind_flags = bind_flags(cfg),
    };
    int ret = xsk_socket__create(&ctx->xsk, cfg->ifname, (uint32_t)cfg->queue_id,
                                 ctx->umem, &ctx->rx, &ctx->tx, &xsk_cfg);
    if (ret) { errno = -ret; perror("xsk_socket__create"); return -1; }
    return 0;
}

/*============================================================
 * fill_initial_umem — 预填充 FILL ring
 *
 * 告诉内核哪些 frame 可以用来接收数据包。
 * 初始填满所有 FILL slot，之后每处理完一个包就归还一个。
 *============================================================*/
static int fill_initial_umem(struct xsk_ctx *ctx)
{
    uint32_t idx;
    int ret = xsk_ring_prod__reserve(&ctx->fq, FILL_RING_SIZE, &idx);
    if (ret != (int)FILL_RING_SIZE) { fprintf(stderr, "fill reserve ret=%d\n", ret); return -1; }
    for (uint32_t i = 0; i < FILL_RING_SIZE; i++)
        *xsk_ring_prod__fill_addr(&ctx->fq, idx + i) = (uint64_t)i * FRAME_SIZE;
    xsk_ring_prod__submit(&ctx->fq, FILL_RING_SIZE);
    return 0;
}

/*============================================================
 * recycle_frame — 归还 frame 到 FILL ring
 *
 * 包处理完后（drop 或 reflect 发完），把 frame 地址还回 FILL ring。
 * 否则内核没有可用 buffer 继续收包。
 *============================================================*/
static int recycle_frame(struct xsk_ctx *ctx, uint64_t addr, struct app_stats *st)
{
    uint32_t idx;
    int ret = xsk_ring_prod__reserve(&ctx->fq, 1, &idx);
    if (ret != 1) return -1;
    *xsk_ring_prod__fill_addr(&ctx->fq, idx) = addr;
    xsk_ring_prod__submit(&ctx->fq, 1);
    st->fill_recycled++;
    return 0;
}

/*============================================================
 * drain_completion — 消费 COMPLETION ring
 *
 * reflect 模式下 sendto() 发包后，内核把已完成的 TX descriptor
 * 放入 COMPLETION ring。这里取出地址，归还到 FILL ring。
 *
 * 注意：COMPLETION ring 返回的是之前 reflect 发出去的地址，
 * 不是 RX ring 的地址。
 *============================================================*/
static void drain_completion(struct xsk_ctx *ctx, struct app_stats *st)
{
    uint32_t idx;
    unsigned int rcvd = xsk_ring_cons__peek(&ctx->cq, BATCH_SIZE, &idx);
    if (!rcvd) return;
    for (unsigned int i = 0; i < rcvd; i++) {
        uint64_t addr = *xsk_ring_cons__comp_addr(&ctx->cq, idx + i);
        if (recycle_frame(ctx, addr, st)) fprintf(stderr, "WARN: recycle completed frame failed\n");
    }
    st->comp_packets += rcvd;
    xsk_ring_cons__release(&ctx->cq, rcvd);
}

/*============================================================
 * tx_reflect_one — 从 TX ring 发送一个包（reflect 模式）
 *
 * 步骤：
 *   1. 从 TX ring 预留一个 slot
 *   2. 写入包的地址和长度（直接用 RX 的 buffer，不复制内容）
 *   3. submit 给内核
 *   4. sendto() 触发内核真正发送
 *   5. 若 TX ring 满了（ret != 1），丢弃并归还 frame
 *
 * sendto() 的作用：
 *   AF_XDP 的 TX 需要用户态主动触发，不像 RX 那样自动推送。
 *   sendto(fd, NULL, 0, MSG_DONTWAIT, NULL, 0) 是一个空发触发，
 *   让内核处理 TX ring 中的待发包。
 *============================================================*/
static int tx_reflect_one(struct xsk_ctx *ctx, uint64_t addr, uint32_t len, struct app_stats *st)
{
    uint32_t idx;
    int ret = xsk_ring_prod__reserve(&ctx->tx, 1, &idx);
    if (ret != 1) {
        // TX ring 满了，丢弃这个包并归还 frame
        st->tx_full_drops++;
        st->dropped_packets++;
        if (recycle_frame(ctx, addr, st)) fprintf(stderr, "WARN: recycle tx-full frame failed\n");
        return -1;
    }
    // 写入 TX descriptor
    struct xdp_desc *tx_desc = xsk_ring_prod__tx_desc(&ctx->tx, idx);
    tx_desc->addr = addr;
    tx_desc->len = len;
    xsk_ring_prod__submit(&ctx->tx, 1);
    st->tx_packets++;
    st->tx_bytes += len;
    // 触发内核发送 TX ring 中的包
    (void)sendto(xsk_socket__fd(ctx->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
    return 0;
}

// 打印统计信息
static void print_stats(const char *tag, const struct app_stats *st)
{
    printf("%s rx_packets=%" PRIu64 " rx_bytes=%" PRIu64
           " tx_packets=%" PRIu64 " tx_bytes=%" PRIu64
           " dropped_packets=%" PRIu64 " fill_recycled=%" PRIu64
           " tx_full_drops=%" PRIu64 " comp_packets=%" PRIu64
           " rx_empty_polls=%" PRIu64 "\n",
           tag, st->rx_packets, st->rx_bytes, st->tx_packets, st->tx_bytes,
           st->dropped_packets, st->fill_recycled, st->tx_full_drops,
           st->comp_packets, st->rx_empty_polls);
    fflush(stdout);
}

/*============================================================
 * load_and_attach_bpf — 加载 BPF object 并 attach XDP program
 *
 * 流程：
 *   1. bpf_object__open_file 打开 BPF ELF
 *   2. bpf_object__load 加载到内核
 *   3. 找到 xdp_sock_prog 和 xsks_map 的 fd
 *   4. bpf_xdp_attach attach 到网卡的 XDP hook
 *   5. 返回 obj 和 xsks_fd（供 register_xsk 使用）
 *============================================================*/
static int load_and_attach_bpf(const struct app_cfg *cfg, int ifindex, struct bpf_object **obj_out, int *xsks_fd_out)
{
    struct bpf_object *obj = bpf_object__open_file(cfg->obj_path, NULL);
    if (!obj) { fprintf(stderr, "bpf_object__open_file failed: %s\n", cfg->obj_path); return -1; }
    int ret = bpf_object__load(obj);
    if (ret) { fprintf(stderr, "bpf_object__load failed: %d\n", ret); bpf_object__close(obj); return -1; }
    struct bpf_program *prog = bpf_object__find_program_by_name(obj, "xdp_sock_prog");
    if (!prog) { fprintf(stderr, "cannot find xdp_sock_prog\n"); bpf_object__close(obj); return -1; }
    int prog_fd = bpf_program__fd(prog);
    int xsks_fd = bpf_object__find_map_fd_by_name(obj, "xsks_map");
    if (prog_fd < 0 || xsks_fd < 0) { fprintf(stderr, "bad bpf fds prog=%d xsks=%d\n", prog_fd, xsks_fd); bpf_object__close(obj); return -1; }
    ret = bpf_xdp_attach(ifindex, prog_fd, xdp_flags(cfg), NULL);
    if (ret) { fprintf(stderr, "bpf_xdp_attach failed ret=%d flags=0x%x\n", ret, xdp_flags(cfg)); bpf_object__close(obj); return -1; }
    *obj_out = obj;
    *xsks_fd_out = xsks_fd;
    return 0;
}

/*============================================================
 * register_xsk — 将 AF_XDP socket fd 注册到 XSKMAP
 *
 * XSKMAP 是 BPF map，key=队列号，value=socket fd。
 * XDP program 通过 rx_queue_index 查 XSKMAP，找到 socket 并 redirect。
 *============================================================*/
static int register_xsk(int xsks_fd, int queue_id, int xsk_fd)
{
    uint32_t key = (uint32_t)queue_id;
    uint32_t val = (uint32_t)xsk_fd;
    if (bpf_map_update_elem(xsks_fd, &key, &val, 0)) { perror("bpf_map_update_elem(xsks_map)"); return -1; }
    return 0;
}

// 主函数：初始化 → 运行主循环 → 清理
int main(int argc, char **argv)
{
    // 默认配置
    struct app_cfg cfg = {
        .ifname = NULL,
        .obj_path = "./af_xdp_forwarder_kern.bpf.o",
        .queue_id = 0,
        .duration_sec = 15,
        .interval_sec = 1,
        .native_mode = false,   // 默认 skb 模式
        .zero_copy = false,    // 默认 copy 模式
        .use_poll = true,
        .mode = FWD_DROP,      // 默认 drop 模式
    };
    struct xsk_ctx xsk = {0};
    struct app_stats stats = {0};
    struct bpf_object *obj = NULL;
    int xsks_fd = -1;
    int ret = 1;

    // 信号处理（优雅停止）
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    // 解析参数
    if (parse_args(argc, argv, &cfg)) return 2;
    int ifindex = if_nametoindex(cfg.ifname);
    if (!ifindex) { perror("if_nametoindex"); return 2; }

    printf("af-xdp-mini-forwarder config: ifname=%s ifindex=%d queue=%d xdp_mode=%s bind=%s forward=%s duration=%d interval=%d\n",
           cfg.ifname, ifindex, cfg.queue_id, cfg.native_mode ? "native" : "skb",
           cfg.zero_copy ? "zero-copy" : "copy", mode_name(cfg.mode), cfg.duration_sec, cfg.interval_sec);

    // 提高 memlock 限制
    if (bump_memlock_rlimit()) return 1;

    // ---- 步骤 1：创建 UMEM ----
    if (create_umem(&xsk)) goto out;
    printf("UMEM_READY frames=%u frame_size=%u bytes=%" PRIu64 "\n", NUM_FRAMES, FRAME_SIZE, (uint64_t)UMEM_SIZE);

    // ---- 步骤 2：创建 AF_XDP socket 并绑定 ----
    if (create_socket(&xsk, &cfg)) goto out;
    printf("XSK_SOCKET_READY fd=%d\n", xsk_socket__fd(xsk.xsk));

    // ---- 步骤 3：预填充 FILL ring ----
    if (fill_initial_umem(&xsk)) goto out;
    printf("FILL_RING_READY descriptors=%u\n", FILL_RING_SIZE);

    // ---- 步骤 4：加载 BPF program 并 attach ----
    if (load_and_attach_bpf(&cfg, ifindex, &obj, &xsks_fd)) goto out;
    printf("XDP_ATTACHED ifname=%s mode=%s\n", cfg.ifname, cfg.native_mode ? "native" : "skb");

    // ---- 步骤 5：注册 socket fd 到 XSKMAP ----
    if (register_xsk(xsks_fd, cfg.queue_id, xsk_socket__fd(xsk.xsk))) goto out_detach;
    printf("XSKMAP_REGISTERED queue=%d fd=%d\n", cfg.queue_id, xsk_socket__fd(xsk.xsk));
    printf("AF_XDP_FORWARDER_READY mode=%s fq=%u cq=%u rx=%u tx=%u\n", mode_name(cfg.mode), FILL_RING_SIZE, COMP_RING_SIZE, RX_RING_SIZE, TX_RING_SIZE);
    printf("enter AF_XDP mini forwarder loop\n");

    // ---- 主循环：poll → RX → 处理(drop/reflect) → TX → drain completion ----
    uint64_t start = now_sec();
    uint64_t next_print = start;
    while (!stop && now_sec() - start < (uint64_t)cfg.duration_sec) {
        if (cfg.use_poll) {
            struct pollfd pfd = { .fd = xsk_socket__fd(xsk.xsk), .events = POLLIN };
            poll(&pfd, 1, 100);  // 100ms 超时
        }

        // 处理 COMPLETION ring（TX 完成通知，reflect 模式需要）
        drain_completion(&xsk, &stats);

        // 从 RX ring 取 descriptor
        uint32_t idx_rx;
        unsigned int rcvd = xsk_ring_cons__peek(&xsk.rx, BATCH_SIZE, &idx_rx);
        if (!rcvd) {
            stats.rx_empty_polls++;
        } else {
            for (unsigned int i = 0; i < rcvd; i++) {
                const struct xdp_desc *desc = xsk_ring_cons__rx_desc(&xsk.rx, idx_rx + i);
                uint64_t addr = xsk_umem__extract_addr(desc->addr);
                uint32_t len = desc->len;
                stats.rx_packets++;
                stats.rx_bytes += len;
                if (cfg.mode == FWD_REFLECT)
                    tx_reflect_one(&xsk, addr, len, &stats);  // reflect：发回
                else {
                    stats.dropped_packets++;                   // drop：丢弃
                    if (recycle_frame(&xsk, addr, &stats)) fprintf(stderr, "WARN: fill recycle failed\n");
                }
            }
            xsk_ring_cons__release(&xsk.rx, rcvd);
        }

        // 定期打印统计
        uint64_t now = now_sec();
        if (now >= next_print) {
            print_stats("FORWARDER_STATS", &stats);
            next_print = now + (uint64_t)cfg.interval_sec;
        }
    }

    printf("leaving AF_XDP mini forwarder loop\n");
    drain_completion(&xsk, &stats);
    print_stats("FORWARDER_FINAL_STATS", &stats);
    ret = 0;

out_detach:
    printf("detaching XDP program\n");
    bpf_xdp_detach(ifindex, xdp_flags(&cfg), NULL);
out:
    if (xsk.xsk) xsk_socket__delete(xsk.xsk);
    if (xsk.umem) xsk_umem__delete(xsk.umem);
    free(xsk.umem_area);
    if (obj) bpf_object__close(obj);
    printf("bye\n");
    return ret;
}