// SPDX-License-Identifier: GPL-2.0
/*
 * af_xdp_rings.c — AF_XDP 用户态收包实验
 *
 * 本程序实现 AF_XDP socket 的最小闭环：
 *
 *   1. 分配 UMEM（用户态数据包缓冲区）
 *   2. 创建 FILL / COMPLETION / RX / TX 四类 ring
 *   3. 创建 AF_XDP socket 并绑定到网卡 + 队列
 *   4. 加载 XDP program 并注册 socket fd 到 XSKMAP
 *   5. poll 循环从 RX ring 取包，并回收 frame 到 FILL ring
 *
 * 这是一个教学性质的最小实现，不含 L2 forwarding 等功能。
 * 下一站会在此基础上扩展转发能力。
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
 * Ubuntu 22.04 自带 libbpf 0.5.0，
 * bpf_xdp_attach / bpf_xdp_detach 是 libbpf 1.0+ 才有的 API。
 * 用旧 API bpf_set_link_xdp_fd() 做兼容。
 *
 * 问题描述：
 *   编译时报错：undefined reference to `bpf_xdp_attach'
 *   原因：libbpf 版本过低（< 1.0），bpf_xdp_attach 尚未实现
 * 解决：
 *   用 bpf_set_link_xdp_fd(ifindex, fd, flags) 替代，
 *   该函数从 libbpf 很早版本就存在，Ubuntu 22.04 可用。
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
 * 有些常量在旧版 libbpf headers 中可能未定义，需要手动提供。
 *============================================================*/

/* NEED_WAKEUP flag：通知内核在 ring 无数据时唤醒进程 */
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
 * UMEM = 用户态分配的连续内存区域，划分为固定大小的 frame。
 * 内核通过 descriptor（frame 地址）而不是实际数据指针来传递数据。
 *============================================================*/

#define FRAME_SIZE 2048U          // 每个 frame 2KB（一个数据包的最大长度）
#define NUM_FRAMES 4096U          // frame 数量（UMEM 总大小 = 8MB）
#define UMEM_SIZE ((uint64_t)FRAME_SIZE * NUM_FRAMES)

#define RX_RING_SIZE 2048U        // RX ring 容量（最多缓存多少个 RX descriptor）
#define TX_RING_SIZE 2048U        // TX ring 容量（最多缓存多少个 TX descriptor）
#define FILL_RING_SIZE 2048U     // FILL ring 容量（预填充可用 frame 数量）
#define COMP_RING_SIZE 2048U     // COMPLETION ring 容量（TX 完成后通知）
#define BATCH_SIZE 64U            // 每次 poll 处理的最大 descriptor 数

/*============================================================
 * 配置结构体
 *
 * 命令行参数解析后的配置，存储网卡名、队列号、运行模式等。
 *============================================================*/
struct app_cfg {
    const char *ifname;          // 网卡名（如 "ens192"）
    const char *obj_path;         // BPF object 文件路径
    int queue_id;                 // RX 队列号（AF_XDP 绑定到哪个队列）
    int duration_sec;            // 运行时间（秒）
    int interval_sec;            // 统计打印间隔（秒）
    bool native_mode;            // XDP 模式：true=native(drv)，false=generic(skb)
    bool zero_copy;             // 是否启用零拷贝（copy vs zero-copy）
    bool use_poll;               // true=poll() 等待，false=busy-loop
};

/*============================================================
 * AF_XDP socket 上下文
 *
 * 封装一个 AF_XDP socket 及其关联的所有资源。
 *============================================================*/
struct xsk_ctx {
    void *umem_area;             // UMEM 起始地址（posix_memalign 分配）
    struct xsk_umem *umem;       // libbpf UMEM 句柄

    /* 四类 ring：
     *   fq (prod): FILL ring — 用户填充可用 buffer 地址，交给内核写入
     *   cq (cons): COMPLETION ring — 内核通知哪些 TX descriptor 已完成
     *   rx (cons): RX ring — 内核放入收到的包描述符
     *   tx (prod): TX ring — 用户放入待发送的包描述符
     */
    struct xsk_ring_prod fq;     // FILL ring（生产者侧）
    struct xsk_ring_cons cq;     // COMPLETION ring（消费者侧）
    struct xsk_socket *xsk;      // AF_XDP socket 句柄
    struct xsk_ring_cons rx;     // RX ring（消费者侧）
    struct xsk_ring_prod tx;     // TX ring（生产者侧）

    uint64_t outstanding_rx;     // 已取出但尚未 recycle 的 frame 数量
};

/*============================================================
 * 应用层统计
 *
 * 记录 AF_XDP socket 运行期间的各项计数器。
 *============================================================*/
struct app_stats {
    uint64_t rx_packets;         // 收到的数据包总数
    uint64_t rx_bytes;           // 收到的总字节数
    uint64_t fill_recycled;      // 回收（recycle）回 FILL ring 的 frame 数量
    uint64_t rx_empty_polls;      // RX ring 为空（无包可收）的 poll 次数
    uint64_t comp_seen;          // COMPLETION ring 中收到的完成通知数
};

/*============================================================
 * 信号处理
 *
 * 收到 SIGINT/SIGTERM 时优雅退出 poll 循环。
 *============================================================*/
static volatile sig_atomic_t stop;

static void on_signal(int signo)
{
    (void)signo;
    stop = 1;
}

/*
 * now_sec — 获取当前时间（单调时钟秒数）
 * 用于计算运行时长和统计打印间隔。
 */
static uint64_t now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec;
}

/*
 * usage — 打印命令行帮助信息
 */
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
 * 参数解析
 *
 * 支持短选项和长选项：
 *   -i/--ifname    网卡名（必填）
 *   -q/--queue     队列号（默认 0）
 *   -d/--duration  运行时间（默认 15 秒）
 *   -t/--interval  统计打印间隔（默认 1 秒）
 *   -m/--mode      skb(generic) 或 native(drv)
 *   -c/--copy      强制 copy 模式
 *   -z/--zero-copy 强制零拷贝模式
 *   -b/--busy-poll  busy loop 代替 poll()
 *   -o/--obj       BPF object 路径
 *============================================================*/
static int parse_args(int argc, char **argv, struct app_cfg *cfg)
{
    static const struct option opts[] = {
        {"ifname",   required_argument, NULL, 'i'},
        {"queue",    required_argument, NULL, 'q'},
        {"duration", required_argument, NULL, 'd'},
        {"interval", required_argument, NULL, 't'},
        {"mode",     required_argument, NULL, 'm'},
        {"copy",     no_argument,       NULL, 'c'},
        {"zero-copy", no_argument,      NULL, 'z'},
        {"busy-poll", no_argument,      NULL, 'b'},
        {"obj",      required_argument, NULL, 'o'},
        {"help",     no_argument,       NULL, 'h'},
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
        case 'c': cfg->zero_copy = false; break;      // copy 模式（显式覆盖）
        case 'z': cfg->zero_copy = true; break;       // zero-copy 模式
        case 'b': cfg->use_poll = false; break;        // busy-loop（不用 poll）
        case 'o': cfg->obj_path = optarg; break;
        case 'h': usage(argv[0]); exit(0);
        default: usage(argv[0]); return -1;
        }
    }

    /* 参数合法性检查：必须指定网卡名，运行时间和间隔必须 > 0 */
    if (!cfg->ifname || cfg->queue_id < 0 || cfg->duration_sec <= 0 || cfg->interval_sec <= 0) {
        usage(argv[0]);
        return -1;
    }
    return 0;
}

/*
 * bump_memlock_rlimit — 解除 RLIMIT_MEMLOCK 限制
 *
 * AF_XDP 需要大块连续内存（UMEM 默认 8MB），
 * 默认的 memlock 限制会导致 mmap 失败。
 */
static int bump_memlock_rlimit(void)
{
    struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
    if (setrlimit(RLIMIT_MEMLOCK, &r)) {
        perror("setrlimit(RLIMIT_MEMLOCK)");
        return -1;
    }
    return 0;
}

/*
 * xdp_flags — 根据配置生成 XDP attach 标志
 *
 * XDP_FLAGS_UPDATE_IF_NOEXIST：只在网卡尚未 attach XDP program 时才 attach
 * XDP_FLAGS_SKB_MODE：通用软件模拟模式（兼容所有网卡）
 * XDP_FLAGS_DRV_MODE：驱动原生模式（需要网卡驱动支持）
 */
static int xdp_flags(const struct app_cfg *cfg)
{
    return XDP_FLAGS_UPDATE_IF_NOEXIST |
           (cfg->native_mode ? XDP_FLAGS_DRV_MODE : XDP_FLAGS_SKB_MODE);
}

/*
 * bind_flags — 生成 AF_XDP socket bind 标志
 *
 * XDP_USE_NEED_WAKEUP：通知内核在 ring 无数据时主动唤醒进程
 * XDP_ZEROCOPY：零拷贝模式（需要网卡和驱动支持）
 * XDP_COPY：拷贝模式（更通用）
 */
static int bind_flags(const struct app_cfg *cfg)
{
    int flags = XDP_USE_NEED_WAKEUP;
    flags |= cfg->zero_copy ? XDP_ZEROCOPY : XDP_COPY;
    return flags;
}

/*============================================================
 * UMEM 创建
 *
 * UMEM 是用户态和内核共享的内存区域，被划分为固定大小的 frame。
 * 内核通过 FILL ring 获得可用 frame，写入数据后放到 RX ring。
 *
 * 流程：
 *   1. posix_memalign 分配 UMEM_SIZE（8MB）对齐内存
 *   2. xsk_umem__create 注册 UMEM，同时指定 FILL 和 COMPLETION ring 大小
 *============================================================*/
static int create_umem(struct xsk_ctx *xsk)
{
    struct xsk_umem_config cfg = {
        .fill_size = FILL_RING_SIZE,    // FILL ring 容量
        .comp_size = COMP_RING_SIZE,    // COMPLETION ring 容量
        .frame_size = FRAME_SIZE,       // 每个 frame 2KB
        .frame_headroom = 0,            // frame 头部留空（用于 meta 信息）
        .flags = 0,
    };

    /* 分配 UMEM 区域（8MB，页对齐）*/
    if (posix_memalign(&xsk->umem_area, getpagesize(), UMEM_SIZE)) {
        perror("posix_memalign(umem)");
        return -1;
    }
    memset(xsk->umem_area, 0, UMEM_SIZE);

    /* 创建 UMEM，关联 FILL/COMPLETION ring */
    if (xsk_umem__create(&xsk->umem, xsk->umem_area, UMEM_SIZE, &xsk->fq, &xsk->cq, &cfg)) {
        perror("xsk_umem__create");
        return -1;
    }
    return 0;
}

/*============================================================
 * AF_XDP socket 创建
 *
 * 创建一个绑定到特定网卡和队列的 AF_XDP socket。
 * 同时分配 RX 和 TX ring。
 *
 * libbpf_flags = XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD：
 *   告诉 libbpf 不要自动加载 XDP program，
 *   因为我们要自己控制 XDP attach。
 *============================================================*/
static int create_socket(struct xsk_ctx *ctx, const struct app_cfg *cfg)
{
    struct xsk_socket_config xsk_cfg = {
        .rx_size = RX_RING_SIZE,        // RX ring 大小
        .tx_size = TX_RING_SIZE,        // TX ring 大小
        .libbpf_flags = XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD,
        .xdp_flags = xdp_flags(cfg),    // XDP attach 模式
        .bind_flags = bind_flags(cfg),   // copy 或 zero-copy
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
 * 预填充 FILL ring
 *
 * 告诉内核："这些 frame 可用，可以往里面写收到的包"。
 *
 * 实现：把 NUM_FRAMES 个 frame 地址全部写入 FILL ring。
 * 每个 frame 地址 = i * FRAME_SIZE（0, 2048, 4096, ...）
 *============================================================*/
static int fill_umem(struct xsk_ctx *ctx)
{
    uint32_t idx;
    const uint32_t fill_count = FILL_RING_SIZE;

    /* 从 FILL ring 申请连续槽位 */
    int ret = xsk_ring_prod__reserve(&ctx->fq, fill_count, &idx);
    if (ret != (int)fill_count) {
        fprintf(stderr, "fill ring reserve failed ret=%d expected=%u\n", ret, fill_count);
        return -1;
    }

    /* 填充所有 frame 地址（每个 frame 起始偏移量）*/
    for (uint32_t i = 0; i < fill_count; i++)
        *xsk_ring_prod__fill_addr(&ctx->fq, idx + i) = (uint64_t)i * FRAME_SIZE;

    /* 提交到 FILL ring，内核现在可以使用这些 buffer */
    xsk_ring_prod__submit(&ctx->fq, fill_count);
    return 0;
}

/*============================================================
 * 回收 frame 到 FILL ring
 *
 * 从 RX ring 取出的 descriptor 使用完毕后，把 frame 地址还给 FILL ring。
 * 这样内核可以再次使用这些 frame 接收新数据包。
 *
 * 注意：frame 地址要用 xsk_umem__extract_addr() 提取原始地址。
 *============================================================*/
static int recycle_rx(struct xsk_ctx *ctx, uint64_t addr, struct app_stats *st)
{
    uint32_t idx;
    int ret = xsk_ring_prod__reserve(&ctx->fq, 1, &idx);
    if (ret != 1)
        return -1;                      // FILL ring 满，跳过本次回收
    *xsk_ring_prod__fill_addr(&ctx->fq, idx) = addr;
    xsk_ring_prod__submit(&ctx->fq, 1);
    st->fill_recycled++;
    return 0;
}

/*============================================================
 * 消费 COMPLETION ring
 *
 * TX 完成后，内核会把 descriptor 放入 COMPLETION ring。
 * 这里只做释放操作（收回 frame），不实际使用返回值。
 *============================================================*/
static void drain_completion(struct xsk_ctx *ctx, struct app_stats *st)
{
    uint32_t idx;
    unsigned int rcvd = xsk_ring_cons__peek(&ctx->cq, BATCH_SIZE, &idx);
    if (!rcvd)
        return;
    st->comp_seen += rcvd;
    xsk_ring_cons__release(&ctx->cq, rcvd);
}

/*
 * print_stats — 打印统计信息
 *
 * 每隔 interval 秒打印一次，便于观察流量变化。
 */
static void print_stats(const char *tag, const struct app_stats *st)
{
    printf("%s rx_packets=%" PRIu64 " rx_bytes=%" PRIu64"
           " fill_recycled=%" PRIu64 " rx_empty_polls=%" PRIu64"
           " comp_seen=%" PRIu64 "\n",
           tag, st->rx_packets, st->rx_bytes, st->fill_recycled,
           st->rx_empty_polls, st->comp_seen);
    fflush(stdout);
}

/*============================================================
 * 加载并 attach XDP program
 *
 * 流程：
 *   1. bpf_object__open_file     — 打开 .bpf.o 文件
 *   2. bpf_object__load          — 加载 maps 和 program 到内核
 *   3. bpf_object__find_program_by_name — 找到 SEC("xdp") 程序
 *   4. bpf_xdp_attach            — attach 到网卡（libbpf 兼容层）
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

    /* 找到 "xdp_sock_prog"（SEC("xdp") 标记的 BPF 程序）*/
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

    /* attach XDP program 到网卡（这里用的是兼容层的 bpf_xdp_attach）*/
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
 * 注册 socket fd 到 XSKMAP
 *
 * 把 AF_XDP socket 的 fd 写入 XSKMAP 的指定队列槽位。
 * XDP program 通过 bpf_redirect_map(&xsks_map, rx_queue_index)
 * 把包重定向到这个 socket。
 *============================================================*/
static int register_xsk(int xsks_fd, int queue_id, int xsk_fd)
{
    uint32_t key = (uint32_t)queue_id;       // 队列号作为 key
    uint32_t val = (uint32_t)xsk_fd;         // socket fd 作为 value
    int ret = bpf_map_update_elem(xsks_fd, &key, &val, 0);
    if (ret) {
        perror("bpf_map_update_elem(xsks_map)");
        return -1;
    }
    return 0;
}

/*============================================================
 * main — 程序入口
 *============================================================*/
int main(int argc, char **argv)
{
    struct app_cfg cfg = {
        .ifname = NULL,
        .obj_path = "./af_xdp_kern.bpf.o",
        .queue_id = 0,
        .duration_sec = 15,
        .interval_sec = 1,
        .native_mode = false,
        .zero_copy = false,
        .use_poll = true,
    };
    struct xsk_ctx xsk = {0};
    struct app_stats stats = {0};
    struct bpf_object *obj = NULL;
    int xsks_fd = -1;
    int ifindex;
    int ret = 1;

    /* 设置信号处理器（支持 Ctrl+C 优雅退出）*/
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    /* 解析命令行参数 */
    if (parse_args(argc, argv, &cfg))
        return 2;

    /* 把网卡名转为 ifindex（内核使用 ifindex 而非 ifname）*/
    ifindex = if_nametoindex(cfg.ifname);
    if (!ifindex) {
        perror("if_nametoindex");
        return 2;
    }

    /* 打印配置信息 */
    printf("af-xdp-rings config: ifname=%s ifindex=%d queue=%d mode=%s bind=%s duration=%d interval=%d\n",
           cfg.ifname, ifindex, cfg.queue_id, cfg.native_mode ? "native" : "skb",
           cfg.zero_copy ? "zero-copy" : "copy", cfg.duration_sec, cfg.interval_sec);

    /* 解除内存锁限制 */
    if (bump_memlock_rlimit())
        return 1;

    /*-----------------------------------------------
     * 1. 创建 UMEM（用户态 packet buffer 区域）
     *-----------------------------------------------*/
    if (create_umem(&xsk))
        goto out;
    printf("UMEM_READY frames=%u frame_size=%u bytes=%" PRIu64 "\n", NUM_FRAMES, FRAME_SIZE, (uint64_t)UMEM_SIZE);

    /*-----------------------------------------------
     * 2. 创建 AF_XDP socket
     *-----------------------------------------------*/
    if (create_socket(&xsk, &cfg))
        goto out;
    printf("XSK_SOCKET_READY fd=%d\n", xsk_socket__fd(xsk.xsk));

    /*-----------------------------------------------
     * 3. 预填充 FILL ring（提供可用 buffer 给内核）
     *-----------------------------------------------*/
    if (fill_umem(&xsk))
        goto out;
    printf("FILL_RING_READY descriptors=%u\n", FILL_RING_SIZE);

    /*-----------------------------------------------
     * 4. 加载 BPF program 并 attach 到网卡
     *-----------------------------------------------*/
    if (load_and_attach_bpf(&cfg, ifindex, &obj, &xsks_fd))
        goto out;
    printf("XDP_ATTACHED ifname=%s mode=%s\n", cfg.ifname, cfg.native_mode ? "native" : "skb");

    /*-----------------------------------------------
     * 5. 把 socket fd 注册到 XSKMAP
     *-----------------------------------------------*/
    if (register_xsk(xsks_fd, cfg.queue_id, xsk_socket__fd(xsk.xsk)))
        goto out_detach;
    printf("XSKMAP_REGISTERED queue=%d fd=%d\n", cfg.queue_id, xsk_socket__fd(xsk.xsk));

    /* 打印所有 ring 状态 */
    printf("AF_XDP_RINGS_READY fq=%u cq=%u rx=%u tx=%u\n",
           FILL_RING_SIZE, COMP_RING_SIZE, RX_RING_SIZE, TX_RING_SIZE);
    printf("enter AF_XDP rx loop\n");

    /*-----------------------------------------------
     * 6. 主循环：poll 收包 → recycle → drain completion
     *-----------------------------------------------*/
    uint64_t start = now_sec();
    uint64_t next_print = start;
    while (!stop && now_sec() - start < (uint64_t)cfg.duration_sec) {
        /* poll 等待（或者 busy-loop）*/
        if (cfg.use_poll) {
            struct pollfd pfd = {
                .fd = xsk_socket__fd(xsk.xsk),
                .events = POLLIN,
            };
            poll(&pfd, 1, 100);
        }

        /* 从 RX ring 取 descriptor（最多 BATCH_SIZE 个）*/
        uint32_t idx_rx;
        unsigned int rcvd = xsk_ring_cons__peek(&xsk.rx, BATCH_SIZE, &idx_rx);
        if (!rcvd) {
            stats.rx_empty_polls++;          // RX ring 为空
        } else {
            for (unsigned int i = 0; i < rcvd; i++) {
                /* 取第 i 个 descriptor */
                const struct xdp_desc *desc = xsk_ring_cons__rx_desc(&xsk.rx, idx_rx + i);
                /* 从 descriptor 中提取 frame 地址（libbpf 内部可能用 offset encoding）*/
                uint64_t addr = xsk_umem__extract_addr(desc->addr);
                stats.rx_packets++;
                stats.rx_bytes += desc->len;
                /* 把用完的 frame 回收给 FILL ring */
                if (recycle_rx(&xsk, addr, &stats))
                    fprintf(stderr, "WARN: fill ring recycle failed\n");
            }
            /* 释放已处理的 descriptor（告诉内核这些可以覆盖了）*/
            xsk_ring_cons__release(&xsk.rx, rcvd);
        }

        /* 处理 TX 完成通知 */
        drain_completion(&xsk, &stats);

        /* 定时打印统计 */
        uint64_t now = now_sec();
        if (now >= next_print) {
            print_stats("AF_XDP_STATS", &stats);
            next_print = now + (uint64_t)cfg.interval_sec;
        }
    }

    printf("leaving AF_XDP rx loop\n");
    print_stats("AF_XDP_FINAL_STATS", &stats);
    ret = 0;

/*-----------------------------------------------
 * 清理出口
 *-----------------------------------------------*/
out_detach:
    printf("detaching XDP program\n");
    bpf_xdp_detach(ifindex, xdp_flags(&cfg), NULL);
out:
    if (xsk.xsk)
        xsk_socket__delete(xsk.xsk);       // 关闭 AF_XDP socket
    if (xsk.umem)
        xsk_umem__delete(xsk.umem);        // 释放 UMEM
    free(xsk.umem_area);                    // 释放内存
    if (obj)
        bpf_object__close(obj);            // 关闭 BPF object
    printf("bye\n");
    return ret;
}