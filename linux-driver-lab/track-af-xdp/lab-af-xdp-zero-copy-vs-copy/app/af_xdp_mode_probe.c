// SPDX-License-Identifier: GPL-2.0
/*
 * af_xdp_mode_probe.c — AF_XDP copy vs zero-copy 模式探测
 *
 * 本程序在 lab-af-xdp-socket-rings 基础上增加模式组合探测：
 *
 *   1. skb + copy      — 通用 XDP，copy 模式（基线）
 *   2. native + copy   — 原生 XDP attach，copy 模式
 *   3. native + zero-copy — 原生 XDP attach，zero-copy 模式
 *   4. skb + zero-copy — 通用 XDP，zero-copy（多数驱动不支持，探测边界）
 *
 * 参数：
 *   --mode skb|native    XDP 挂载模式（决定走 generic 还是 drv 路径）
 *   --copy               请求 copy 模式（默认）
 *   --zero-copy          请求 zero-copy 模式（依赖网卡驱动支持）
 *
 * 预期结果：
 *   在 VMware vmxnet3 上：skb+copy 稳定，native/zero-copy 可能失败
 *   失败本身也是有价值的记录，说明该驱动的 AF_XDP 支持边界
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
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

#include <linux/if_link.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <bpf/xsk.h>

/*============================================================
 * libbpf 版本兼容层
 *
 * 问题：Ubuntu 22.04 自带 libbpf 0.5.0，
 * bpf_xdp_attach / bpf_xdp_detach 是 libbpf 1.0+ 才有的 API。
 * 编译时直接报错：undefined reference to `bpf_xdp_attach'
 *
 * 解决：用 bpf_set_link_xdp_fd() 替代，两者语义相同，兼容性更好。
 *============================================================*/
#if LIBBPF_VERSION < 100
#define bpf_xdp_attach(ifindex, prog_fd, xdp_flags, _) \
    bpf_set_link_xdp_fd(ifindex, prog_fd, xdp_flags)
#define bpf_xdp_detach(ifindex, xdp_flags, _) \
    bpf_set_link_xdp_fd(ifindex, -1, xdp_flags)
#endif

/*============================================================
 * XDP / AF_XDP 常量兼容层
 *
 * XDP_USE_NEED_WAKEUP：通知内核在 ring 无数据时唤醒进程，
 * 可以避免 busy-poll 时的空转，提高效率。
 *============================================================*/
#ifndef XDP_USE_NEED_WAKEUP
#define XDP_USE_NEED_WAKEUP (1 << 3)
#endif

/* 抑制 libbpf 自动加载 XDP program（因为我们要自己 attach）*/
#ifndef XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD
#define XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD (1 << 0)
#endif

/*============================================================
 * UMEM / Ring 大小配置
 *
 * 每个 frame 2KB，4096 个 frame，总共 8MB UMEM。
 * 实际使用中 frame size应大于 MTU（1500），2KB 足够。
 *============================================================*/
#define FRAME_SIZE 2048U          // 每个 frame 大小（字节）
#define NUM_FRAMES 4096U          // frame 总数
#define UMEM_SIZE ((uint64_t)FRAME_SIZE * NUM_FRAMES)  // UMEM 总大小：8MB

#define RX_RING_SIZE 2048U        // RX ring 容量
#define TX_RING_SIZE 2048U        // TX ring 容量
#define FILL_RING_SIZE 2048U     // FILL ring 容量（预填充可用 frame）
#define COMP_RING_SIZE 2048U     // COMPLETION ring 容量（TX 完成通知）
#define BATCH_SIZE 64U            // 每次 poll 处理的最大 descriptor 数

/*============================================================
 * app_cfg — 命令行参数配置结构
 *
 * 通过 getopt_long 解析，支持短选项和长选项。
 *============================================================*/
struct app_cfg {
    const char *ifname;          // 网卡名（如 "ens192"）
    const char *obj_path;         // BPF object 文件路径
    int queue_id;                 // 绑定到的 RX 队列号
    int duration_sec;            // 程序运行时间（秒）
    int interval_sec;            // 统计打印间隔（秒）
    bool native_mode;            // true = XDP_FLAGS_DRV_MODE（native）
                                // false = XDP_FLAGS_SKB_MODE（generic）
    bool zero_copy;             // true = XDP_ZEROCOPY，false = XDP_COPY
    bool use_poll;               // true = poll() 等待，false = busy loop
};

/*============================================================
 * xsk_ctx — AF_XDP socket 上下文
 *
 * 封装一个 AF_XDP socket 及其关联的所有资源：
 *   umem_area / umem    — UMEM 内存区域
 *   fq                  — FILL ring（生产者侧）
 *   cq                  — COMPLETION ring（消费者侧）
 *   rx                  — RX ring（消费者侧，接收内核传来的包描述符）
 *   tx                  — TX ring（生产者侧，发送时放入待发包描述符）
 *   xsk                 — AF_XDP socket 句柄
 *   outstanding_rx      — 已分配但尚未归还的 RX descriptor 计数
 *============================================================*/
struct xsk_ctx {
    void *umem_area;             // UMEM 起始地址（posix_memalign 分配）
    struct xsk_umem *umem;       // libbpf UMEM 句柄

    /* 四类 ring：
     *   fq (prod): FILL ring  — 用户填充可用 frame 地址，交给内核写入
     *   cq (cons): COMPLETION ring — 内核通知哪些 TX descriptor 已完成
     *   rx (cons): RX ring    — 内核放入收到的包描述符，用户取出读取
     *   tx (prod): TX ring    — 用户放入待发送的包描述符，内核取出发送
     */
    struct xsk_ring_prod fq;    // FILL ring（生产者）
    struct xsk_ring_cons cq;    // COMPLETION ring（消费者）
    struct xsk_ring_cons rx;    // RX ring（消费者）
    struct xsk_ring_prod tx;    // TX ring（生产者）
    struct xsk_socket *xsk;     // AF_XDP socket 句柄

    uint64_t outstanding_rx;    // 已取走但尚未归还 FILL 的 descriptor 数
};

/*============================================================
 * app_stats — 应用层统计
 *
 * 所有字段均为无锁设计，配合 per-CPU 或原子操作使用。
 *============================================================*/
struct app_stats {
    uint64_t rx_packets;         // 收到的数据包总数
    uint64_t rx_bytes;           // 收到的总字节数
    uint64_t fill_recycled;      // 归还到 FILL ring 的 frame 数
    uint64_t rx_empty_polls;    // poll 到空数据的次数（反映无流量时的轮询）
    uint64_t comp_seen;         // COMPLETION ring 中收到的完成通知数
};

// 全局停止标志，由信号处理程序设置
static volatile sig_atomic_t stop;

// 信号处理函数：收到 SIGINT/SIGTERM 时优雅停止
static void on_signal(int signo)
{
    (void)signo;
    stop = 1;
}

// 获取当前时间戳（秒），用于计时和间隔打印
static uint64_t now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec;
}

// 打印用法说明
static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s --ifname IFACE [options]\n"
            "\n"
            "Options:\n"
            "  --queue N          RX queue id, default 0\n"
            "  --duration SEC     run duration, default 15\n"
            "  --interval SEC     stats interval, default 1\n"
            "  --mode skb|native   XDP attach mode, default skb\n"
            "  --copy             request XDP copy mode, default for skb lab\n"
            "  --zero-copy        request XDP zero-copy mode, may fail on unsupported NIC\n"
            "  --busy-poll        busy loop instead of poll(2)\n"
            "  --obj PATH         BPF object path, default ./af_xdp_kern.bpf.o\n",
            prog);
}

/*============================================================
 * parse_args — 解析命令行参数
 *
 * 支持短选项和长选项，解析后填充 app_cfg 结构。
 * 返回 0 表示成功，-1 表示失败（参数错误或 --help）。
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
        {"busy-poll", no_argument, NULL, 'b'},
        {"obj", required_argument, NULL, 'o'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0},
    };

    int c;
    while ((c = getopt_long(argc, argv, "i:q:d:t:m:czo:bh", opts, NULL)) != -1) {
        switch (c) {
        case 'i': cfg->ifname = optarg; break;
        case 'q': cfg->queue_id = atoi(optarg); break;
        case 'd': cfg->duration_sec = atoi(optarg); break;
        case 't': cfg->interval_sec = atoi(optarg); break;
        case 'm':
            if (strcmp(optarg, "native") == 0)
                cfg->native_mode = true;
            else if (strcmp(optarg, "skb") == 0)
                cfg->native_mode = false;
            else {
                fprintf(stderr, "invalid --mode %s\n", optarg);
                return -1;
            }
            break;
        case 'c': cfg->zero_copy = false; break;   // --copy → copy 模式
        case 'z': cfg->zero_copy = true; break;    // --zero-copy → zero-copy 模式
        case 'b': cfg->use_poll = false; break;    // --busy-poll → busy loop
        case 'o': cfg->obj_path = optarg; break;
        case 'h': usage(argv[0]); exit(0);
        default: usage(argv[0]); return -1;
        }
    }

    // 必须提供 --ifname，时间和间隔必须为正
    if (!cfg->ifname || cfg->queue_id < 0 || cfg->duration_sec <= 0 || cfg->interval_sec <= 0) {
        usage(argv[0]);
        return -1;
    }
    return 0;
}

// 提高 RLIMIT_MEMLOCK 限制，允许锁定大量内存（UMEM 分配需要）
static int bump_memlock_rlimit(void)
{
    struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
    if (setrlimit(RLIMIT_MEMLOCK, &r)) {
        perror("setrlimit(RLIMIT_MEMLOCK)");
        return -1;
    }
    return 0;
}

/*============================================================
 * xdp_flags — 根据模式生成 XDP attach 标志
 *
 * XDP_FLAGS_UPDATE_IF_NOEXIST — 若已存在 XDP program，不替换
 * XDP_FLAGS_SKB_MODE          — generic XDP（通用，适合所有驱动）
 * XDP_FLAGS_DRV_MODE          — native XDP（需要驱动支持）
 *============================================================*/
static int xdp_flags(const struct app_cfg *cfg)
{
    return XDP_FLAGS_UPDATE_IF_NOEXIST |
           (cfg->native_mode ? XDP_FLAGS_DRV_MODE : XDP_FLAGS_SKB_MODE);
}

/*============================================================
 * bind_flags — 生成 AF_XDP bind 标志
 *
 * XDP_USE_NEED_WAKEUP — 告诉内核在 ring 无数据时阻塞进程（节省 CPU）
 * XDP_ZEROCOPY        — 要求 zero-copy（依赖网卡和驱动支持）
 * XDP_COPY            — 使用 copy 模式（兼容性强）
 *============================================================*/
static int bind_flags(const struct app_cfg *cfg)
{
    int flags = XDP_USE_NEED_WAKEUP;
    flags |= cfg->zero_copy ? XDP_ZEROCOPY : XDP_COPY;
    return flags;
}

/*============================================================
 * create_umem — 创建 UMEM
 *
 * 步骤：
 *   1. posix_memalign 对齐分配 8MB 内存
 *   2. 清零内存区域
 *   3. xsk_umem__create 创建 libbpf UMEM 对象
 *   4. 将 fq（fill ring）和 cq（completion ring）的句柄传回
 *
 * 注意：UMEM 一旦创建，fq/cq 就已经绑定了，
 * 后续不能单独修改，需要通过 xsk_umem__delete 销毁。
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

    if (posix_memalign(&xsk->umem_area, getpagesize(), UMEM_SIZE)) {
        perror("posix_memalign(umem)");
        return -1;
    }
    memset(xsk->umem_area, 0, UMEM_SIZE);

    if (xsk_umem__create(&xsk->umem, xsk->umem_area, UMEM_SIZE, &xsk->fq, &xsk->cq, &cfg)) {
        perror("xsk_umem__create");
        return -1;
    }
    return 0;
}

/*============================================================
 * create_socket — 创建 AF_XDP socket 并绑定
 *
 * 步骤：
 *   1. 配置 xsk_socket_config（rx/tx ring 大小、xdp_flags、bind_flags）
 *   2. xsk_socket__create 创建 socket 并绑定到网卡的指定队列
 *
 * 参数解释：
 *   libbpf_flags = XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD
 *     告诉 libbpf 不要自动加载 BPF program，由我们手动加载
 *   xdp_flags    — 见 xdp_flags()
 *   bind_flags   — 见 bind_flags()
 *
 * 返回值：0 成功，-1 失败（错误码存在 errno）
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
    if (ret) {
        errno = -ret;
        perror("xsk_socket__create");
        return -1;
    }
    return 0;
}

/*============================================================
 * fill_umem — 预填充 FILL ring
 *
 * 将 NUM_FRAMES 个可用 frame 地址放入 FILL ring，
 * 告诉内核："这些 buffer 可以用来接收数据包"。
 *
 * 流程：
 *   1. xsk_ring_prod__reserve 预留 FILL_RING_SIZE 个 slot
 *   2. 写入每个 frame 的起始地址（i * FRAME_SIZE）
 *   3. xsk_ring_prod__submit 提交，内核可见
 *
 * 注意：frame 地址必须按 FRAME_SIZE 对齐，
 * 地址 = frame_index * FRAME_SIZE。
 *============================================================*/
static int fill_umem(struct xsk_ctx *ctx)
{
    uint32_t idx;
    const uint32_t fill_count = FILL_RING_SIZE;
    int ret = xsk_ring_prod__reserve(&ctx->fq, fill_count, &idx);
    if (ret != (int)fill_count) {
        fprintf(stderr, "fill ring reserve failed ret=%d expected=%u\n", ret, fill_count);
        return -1;
    }

    for (uint32_t i = 0; i < fill_count; i++)
        *xsk_ring_prod__fill_addr(&ctx->fq, idx + i) = (uint64_t)i * FRAME_SIZE;

    xsk_ring_prod__submit(&ctx->fq, fill_count);
    return 0;
}

/*============================================================
 * recycle_rx — 归还 RX descriptor 到 FILL ring
 *
 * 用户从 RX ring 取走一个 frame 处理完后，
 * 必须把这个 frame 地址归还给 FILL ring，
 * 否则内核没有可用 buffer，继续收包。
 *
 * 这是一个最小化的"每包回收"实现。
 * 生产环境中可以用批量回收（一次 reserve/submit 多个）提升效率。
 *============================================================*/
static int recycle_rx(struct xsk_ctx *ctx, uint64_t addr, struct app_stats *st)
{
    uint32_t idx;
    int ret = xsk_ring_prod__reserve(&ctx->fq, 1, &idx);
    if (ret != 1)
        return -1;
    *xsk_ring_prod__fill_addr(&ctx->fq, idx) = addr;
    xsk_ring_prod__submit(&ctx->fq, 1);
    st->fill_recycled++;
    return 0;
}

// drain_completion — 消费 COMPLETION ring 中的完成通知
//
// TX 发包后，内核把已完成的 descriptor 放入 COMPLETION ring。
// 这里用 BATCH_SIZE 为单位 peek，release 释放引用。
// 当前 lab 不做 TX 发送，这个函数主要是完整 ring 闭环。
static void drain_completion(struct xsk_ctx *ctx, struct app_stats *st)
{
    uint32_t idx;
    unsigned int rcvd = xsk_ring_cons__peek(&ctx->cq, BATCH_SIZE, &idx);
    if (!rcvd)
        return;
    st->comp_seen += rcvd;
    xsk_ring_cons__release(&ctx->cq, rcvd);
}

// 打印统计信息，tag 是前缀（如 "AF_XDP_STATS"）
static void print_stats(const char *tag, const struct app_stats *st)
{
    printf("%s rx_packets=%" PRIu64 " rx_bytes=%" PRIu64
           " fill_recycled=%" PRIu64 " rx_empty_polls=%" PRIu64
           " comp_seen=%" PRIu64 "\n",
           tag, st->rx_packets, st->rx_bytes, st->fill_recycled,
           st->rx_empty_polls, st->comp_seen);
    fflush(stdout);
}

/*============================================================
 * load_and_attach_bpf — 加载 BPF object 并 attach XDP program
 *
 * 流程：
 *   1. bpf_object__open_file 打开 BPF ELF 文件
 *   2. bpf_object__load 加载所有 map 和 program 到内核
 *   3. 找到 xdp_sock_prog program 和 xsks_map 的 fd
 *   4. bpf_xdp_attach 将 program attach 到网卡的 XDP hook
 *   5. 返回 obj 和 xsks_fd（供 register_xsk 使用）
 *============================================================*/
static int load_and_attach_bpf(const struct app_cfg *cfg, int ifindex,
                               struct bpf_object **obj_out, int *xsks_fd_out)
{
    struct bpf_object *obj = NULL;
    struct bpf_program *prog;
    int prog_fd;
    int xsks_fd;
    int ret;

    obj = bpf_object__open_file(cfg->obj_path, NULL);
    if (!obj) {
        fprintf(stderr, "bpf_object__open_file failed: %s\n", cfg->obj_path);
        return -1;
    }

    ret = bpf_object__load(obj);
    if (ret) {
        fprintf(stderr, "bpf_object__load failed: %d\n", ret);
        bpf_object__close(obj);
        return -1;
    }

    prog = bpf_object__find_program_by_name(obj, "xdp_sock_prog");
    if (!prog) {
        fprintf(stderr, "cannot find BPF program xdp_sock_prog\n");
        bpf_object__close(obj);
        return -1;
    }

    prog_fd = bpf_program__fd(prog);
    xsks_fd = bpf_object__find_map_fd_by_name(obj, "xsks_map");
    if (prog_fd < 0 || xsks_fd < 0) {
        fprintf(stderr, "cannot get BPF fds prog_fd=%d xsks_fd=%d\n", prog_fd, xsks_fd);
        bpf_object__close(obj);
        return -1;
    }

    ret = bpf_xdp_attach(ifindex, prog_fd, xdp_flags(cfg), NULL);
    if (ret) {
        fprintf(stderr, "bpf_xdp_attach failed ret=%d ifindex=%d flags=0x%x\n", ret, ifindex, xdp_flags(cfg));
        bpf_object__close(obj);
        return -1;
    }

    *obj_out = obj;
    *xsks_fd_out = xsks_fd;
    return 0;
}

/*============================================================
 * register_xsk — 将 AF_XDP socket fd 注册到 XSKMAP
 *
 * XSKMAP 是 BPF map，key=队列号，value=socket fd。
 * XDP program 通过 rx_queue_index 查 XSKMAP，找到对应的 socket，
 * 然后 bpf_redirect_map 跳转过去。
 *
 * 这一步是 XDP redirect 的核心：把 socket 和 XDP hook 关联起来。
 *============================================================*/
static int register_xsk(int xsks_fd, int queue_id, int xsk_fd)
{
    uint32_t key = (uint32_t)queue_id;
    uint32_t val = (uint32_t)xsk_fd;
    int ret = bpf_map_update_elem(xsks_fd, &key, &val, 0);
    if (ret) {
        perror("bpf_map_update_elem(xsks_map)");
        return -1;
    }
    return 0;
}

// 主函数：初始化 → 运行 → 清理
int main(int argc, char **argv)
{
    // 默认配置
    struct app_cfg cfg = {
        .ifname = NULL,
        .obj_path = "./af_xdp_kern.bpf.o",
        .queue_id = 0,
        .duration_sec = 15,
        .interval_sec = 1,
        .native_mode = false,    // 默认 skb 模式
        .zero_copy = false,     // 默认 copy 模式
        .use_poll = true,
    };
    struct xsk_ctx xsk = {0};
    struct app_stats stats = {0};
    struct bpf_object *obj = NULL;
    int xsks_fd = -1;
    int ifindex;
    int ret = 1;

    // 注册信号处理（优雅停止）
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    // 解析参数
    if (parse_args(argc, argv, &cfg))
        return 2;

    // 获取网卡的 ifindex（BPF API 需要）
    ifindex = if_nametoindex(cfg.ifname);
    if (!ifindex) {
        perror("if_nametoindex");
        return 2;
    }

    // 打印配置
    printf("af-xdp-rings config: ifname=%s ifindex=%d queue=%d mode=%s bind=%s duration=%d interval=%d\n",
           cfg.ifname, ifindex, cfg.queue_id, cfg.native_mode ? "native" : "skb",
           cfg.zero_copy ? "zero-copy" : "copy", cfg.duration_sec, cfg.interval_sec);

    // 提高 memlock 限制（UMEM 分配需要）
    if (bump_memlock_rlimit())
        return 1;

    // ---- 步骤 1：创建 UMEM ----
    if (create_umem(&xsk))
        goto out;
    printf("UMEM_READY frames=%u frame_size=%u bytes=%" PRIu64 "\n", NUM_FRAMES, FRAME_SIZE, (uint64_t)UMEM_SIZE);

    // ---- 步骤 2：创建 AF_XDP socket 并绑定 ----
    if (create_socket(&xsk, &cfg))
        goto out;
    printf("XSK_SOCKET_READY fd=%d\n", xsk_socket__fd(xsk.xsk));

    // ---- 步骤 3：预填充 FILL ring ----
    if (fill_umem(&xsk))
        goto out;
    printf("FILL_RING_READY descriptors=%u\n", FILL_RING_SIZE);

    // ---- 步骤 4：加载 BPF program 并 attach 到 XDP hook ----
    if (load_and_attach_bpf(&cfg, ifindex, &obj, &xsks_fd))
        goto out;
    printf("XDP_ATTACHED ifname=%s mode=%s\n", cfg.ifname, cfg.native_mode ? "native" : "skb");

    // ---- 步骤 5：注册 socket fd 到 XSKMAP ----
    if (register_xsk(xsks_fd, cfg.queue_id, xsk_socket__fd(xsk.xsk)))
        goto out_detach;
    printf("XSKMAP_REGISTERED queue=%d fd=%d\n", cfg.queue_id, xsk_socket__fd(xsk.xsk));

    printf("AF_XDP_RINGS_READY fq=%u cq=%u rx=%u tx=%u\n",
           FILL_RING_SIZE, COMP_RING_SIZE, RX_RING_SIZE, TX_RING_SIZE);
    printf("enter AF_XDP rx loop\n");

    // ---- 主循环：poll 收包 → 回收 → drain completion ----
    uint64_t start = now_sec();
    uint64_t next_print = start;
    while (!stop && now_sec() - start < (uint64_t)cfg.duration_sec) {
        if (cfg.use_poll) {
            struct pollfd pfd = {
                .fd = xsk_socket__fd(xsk.xsk),
                .events = POLLIN,
            };
            poll(&pfd, 1, 100);  // 100ms 超时
        }

        // ---- 从 RX ring 取 descriptor ----
        uint32_t idx_rx;
        unsigned int rcvd = xsk_ring_cons__peek(&xsk.rx, BATCH_SIZE, &idx_rx);
        if (!rcvd) {
            stats.rx_empty_polls++;
        } else {
            for (unsigned int i = 0; i < rcvd; i++) {
                const struct xdp_desc *desc = xsk_ring_cons__rx_desc(&xsk.rx, idx_rx + i);
                // 从 desc->addr 提取 frame 地址（注意 libbpf 会用 addr & ~15 做掩码）
                uint64_t addr = xsk_umem__extract_addr(desc->addr);
                stats.rx_packets++;
                stats.rx_bytes += desc->len;
                // 归还 frame 到 FILL ring
                if (recycle_rx(&xsk, addr, &stats))
                    fprintf(stderr, "WARN: fill ring recycle failed\n");
            }
            xsk_ring_cons__release(&xsk.rx, rcvd);
        }

        // ---- 处理 COMPLETION ring（TX 完成通知）----
        drain_completion(&xsk, &stats);

        // ---- 定期打印统计 ----
        uint64_t now = now_sec();
        if (now >= next_print) {
            print_stats("AF_XDP_STATS", &stats);
            next_print = now + (uint64_t)cfg.interval_sec;
        }
    }

    printf("leaving AF_XDP rx loop\n");
    print_stats("AF_XDP_FINAL_STATS", &stats);
    ret = 0;

// 跳转标签：清理（从 create_umem 失败时直接跳到这里）
out_detach:
    printf("detaching XDP program\n");
    bpf_xdp_detach(ifindex, xdp_flags(&cfg), NULL);
out:
    if (xsk.xsk)
        xsk_socket__delete(xsk.xsk);
    if (xsk.umem)
        xsk_umem__delete(xsk.umem);
    free(xsk.umem_area);
    if (obj)
        bpf_object__close(obj);
    printf("bye\n");
    return ret;
}