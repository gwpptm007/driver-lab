// SPDX-License-Identifier: GPL-2.0
/*
 * xdp_loader.c
 *
 * libbpf 加载器，用于 lab-xdp-redirect-basics
 *
 * 功能：
 *   1. 加载 BPF object（.bpf.o）
 *   2. 把 XDP program attach 到网卡
 *   3. 通过 config_map 控制 action
 *   4. 定期读取 stats_map 打印统计
 *
 * 典型用法：
 *   sudo ./xdp_loader run --ifname ens192 --mode skb --action pass --duration 10
 *   sudo ./xdp_loader detach --ifname ens192 --mode skb
 */
#include <errno.h>
#include <getopt.h>
#include <linux/bpf.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

/*============================================================
 * libbpf 版本兼容层
 *
 * Ubuntu 22.04 自带 libbpf 0.5.0，
 * bpf_xdp_attach / bpf_xdp_detach 是 libbpf 1.0+ 才有的 API。
 * 用旧 API bpf_set_link_xdp_fd() 做兼容。
 *============================================================*/
#if LIBBPF_VERSION < 100
#define bpf_xdp_attach(ifindex, prog_fd, xdp_flags, _) \
    bpf_set_link_xdp_fd(ifindex, prog_fd, xdp_flags)
#define bpf_xdp_detach(ifindex, xdp_flags, _) \
    bpf_set_link_xdp_fd(ifindex, -1, xdp_flags)
#endif

/*
 * 统计结构（与 BPF 程序中 struct xdp_action_stat 对应）
 * 用户态从 stats_map 读取并汇总所有 CPU 的值
 */
struct xdp_action_stat {
    uint64_t packets;
    uint64_t bytes;
};

/*============================================================
 * 信号处理
 *============================================================*/

static volatile sig_atomic_t stop;

/*
 * on_signal — 收到 SIGINT/SIGTERM 时优雅退出
 * set rlimit 之前就设置好信号处理，确保能清理 XDP attach
 */
static void on_signal(int signo)
{
    (void)signo;
    stop = 1;
}

/*============================================================
 * 错误处理
 *============================================================*/

/*
 * die — 打印错误信息到 stderr 并退出
 * 类似 assert，但用于可恢复的错误
 */
static void die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

/*============================================================
 * 辅助函数
 *============================================================*/

/*
 * action_name — 将 XDP action 数值转为字符串
 */
static const char *action_name(uint32_t action)
{
    switch (action) {
    case XDP_ABORTED:  return "aborted";
    case XDP_DROP:     return "drop";
    case XDP_PASS:     return "pass";
    case XDP_TX:       return "tx";
    case XDP_REDIRECT: return "redirect";
    default:           return "unknown";
    }
}

/*
 * parse_action — 把命令行 "pass"/"drop"/"redirect" 转为 XDP action 值
 */
static int parse_action(const char *s, uint32_t *action)
{
    if (!strcmp(s, "pass")) {
        *action = XDP_PASS;
        return 0;
    }
    if (!strcmp(s, "drop")) {
        *action = XDP_DROP;
        return 0;
    }
    if (!strcmp(s, "redirect")) {
        *action = XDP_REDIRECT;
        return 0;
    }
    return -EINVAL;
}

/*
 * parse_xdp_flags — 把 "skb"/"drv"/"hw" 转为 XDP_FLAGS_* 常量
 *
 * 模式说明：
 *   skb (generic): 软件模拟，兼容所有网卡
 *   drv (native):  驱动原生支持，需要驱动实现 XDP
 *   hw (offload):  网卡硬件 offload
 */
static int parse_xdp_flags(const char *mode)
{
    if (!strcmp(mode, "skb") || !strcmp(mode, "generic"))
        return XDP_FLAGS_SKB_MODE;
    if (!strcmp(mode, "drv") || !strcmp(mode, "native"))
        return XDP_FLAGS_DRV_MODE;
    if (!strcmp(mode, "hw"))
        return XDP_FLAGS_HW_MODE;
    return -EINVAL;
}

/*
 * bump_memlock_rlimit — 解除 RLIMIT_MEMLOCK 限制
 *
 * BPF 和 XDP 需要分配大块内存（用于 map、UMEM 等），
 * 默认的 memlock 限制太小，加载会失败。
 * 设置成 RLIM_INFINITY 可以解锁。
 */
static void bump_memlock_rlimit(void)
{
    struct rlimit rlim = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &rlim))
        fprintf(stderr, "WARN: setrlimit(RLIMIT_MEMLOCK) failed: %s\n", strerror(errno));
}

/*
 * align8 — 8 字节对齐
 *
 * BPF per-CPU map 的 value 布局需要 8 字节对齐。
 * 每个 CPU 的值在 values 数组中的偏移 = cpu * align8(sizeof(value))
 */
static int align8(int v)
{
    return (v + 7) & ~7;
}

/*
 * read_one_stat — 读取并汇总一个 key 的 per-CPU 统计
 *
 * map_fd: stats_map 的文件描述符
 * ncpus:  CPU 核心数
 * key:    要查询的 action（XDP_DROP/PASS/REDIRECT）
 * out:    输出统计结果（所有 CPU 累加后的值）
 *
 * 原理：PERCPU_ARRAY 每个 CPU 有一份独立的值，
 *       查一次得到所有 CPU 的数组，再逐个累加。
 */
static void read_one_stat(int map_fd, int ncpus, uint32_t key, struct xdp_action_stat *out)
{
    int value_sz = align8((int)sizeof(struct xdp_action_stat));
    /*
     * calloc：分配 ncpus * value_sz 字节，并清零
     * 注意：不能用 malloc，因为 per-CPU value 必须初始化为 0
     */
    unsigned char *values = calloc((size_t)ncpus, (size_t)value_sz);

    if (!values)
        die("calloc stats values failed");

    memset(out, 0, sizeof(*out));

    /* bpf_map_lookup_elem：查 stats_map[key]，返回所有 CPU 的值数组 */
    if (bpf_map_lookup_elem(map_fd, &key, values) != 0) {
        fprintf(stderr, "WARN: lookup stats key=%u failed: %s\n", key, strerror(errno));
        free(values);
        return;
    }

    /* 累加所有 CPU 的值 */
    for (int cpu = 0; cpu < ncpus; cpu++) {
        struct xdp_action_stat *v = (struct xdp_action_stat *)(values + cpu * value_sz);
        out->packets += v->packets;
        out->bytes += v->bytes;
    }

    free(values);
}

/*
 * print_stats — 打印所有 action 的统计
 *
 * 每隔 --interval 秒调用一次，
 * 遍历 DROP / PASS / REDIRECT 三个 key，打印累计值
 */
static void print_stats(int stats_fd, int ncpus)
{
    uint32_t keys[] = { XDP_DROP, XDP_PASS, XDP_REDIRECT };
    time_t now = time(NULL);

    printf("\n=== xdp stats @ %ld ===\n", (long)now);
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        struct xdp_action_stat s;
        read_one_stat(stats_fd, ncpus, keys[i], &s);
        printf("action=%-8s key=%u packets=%llu bytes=%llu\n",
               action_name(keys[i]), keys[i],
               (unsigned long long)s.packets,
               (unsigned long long)s.bytes);
    }
    fflush(stdout);
}

/*
 * usage — 打印帮助信息
 */
static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s run --ifname IFACE [--mode skb|drv] [--action pass|drop|redirect] [--duration SEC] [--interval SEC] [--obj FILE]\n"
            "  %s detach --ifname IFACE [--mode skb|drv]\n"
            "\n"
            "Examples:\n"
            "  sudo %s run --ifname ens192 --mode skb --action pass --duration 10\n"
            "  sudo %s run --ifname ens192 --mode skb --action drop --duration 5\n"
            "  sudo %s detach --ifname ens192 --mode skb\n",
            prog, prog, prog, prog, prog);
}

/*============================================================
 * main — 程序入口
 *============================================================*/

int main(int argc, char **argv)
{
    const char *cmd = NULL;         // "run" 或 "detach"
    const char *ifname = NULL;      // 网卡名，如 "ens192"
    const char *mode = "skb";       // XDP 模式（默认 skb）
    const char *action_str = "pass"; // action 字符串
    const char *obj_path = "xdp_redirect_basics.bpf.o"; // BPF object 路径
    int duration = 10;              // 运行时间（秒）
    int interval = 1;                // 统计打印间隔（秒）
    uint32_t action = XDP_PASS;
    int xdp_flags;
    int ifindex;

    /*-----------------------------------------------
     * 命令行参数解析（getopt_long）
     * 支持短选项 -i -m -a -d -t -o -h
     * 和长选项 --ifname --mode --action 等
     *-----------------------------------------------*/
    static const struct option long_opts[] = {
        {"ifname",   required_argument, NULL, 'i'},
        {"mode",     required_argument, NULL, 'm'},
        {"action",   required_argument, NULL, 'a'},
        {"duration", required_argument, NULL, 'd'},
        {"interval", required_argument, NULL, 't'},
        {"obj",      required_argument, NULL, 'o'},
        {"help",     no_argument,       NULL, 'h'},
        {0, 0, 0, 0},
    };

    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    cmd = argv[1];                  // 第一个参数是 "run" 或 "detach"
    optind = 2;                     // getopt 从索引 2 开始

    for (;;) {
        int c = getopt_long(argc, argv, "i:m:a:d:t:o:h", long_opts, NULL);
        if (c == -1)
            break;
        switch (c) {
        case 'i': ifname = optarg; break;
        case 'm': mode = optarg; break;
        case 'a': action_str = optarg; break;
        case 'd': duration = atoi(optarg); break;
        case 't': interval = atoi(optarg); break;
        case 'o': obj_path = optarg; break;
        case 'h':
        default:
            usage(argv[0]);
            return c == 'h' ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }

    /* 必填参数：网卡名 */
    if (!ifname)
        die("--ifname is required");

    /* 把网卡名转为 ifindex（内核用 ifindex 而不是 ifname）*/
    ifindex = (int)if_nametoindex(ifname);
    if (!ifindex)
        die("if_nametoindex(%s) failed: %s", ifname, strerror(errno));

    /* 解析 XDP 模式 */
    xdp_flags = parse_xdp_flags(mode);
    if (xdp_flags < 0)
        die("invalid --mode: %s", mode);

    /*-----------------------------------------------
     * detach 命令：直接从网卡卸下 XDP program
     *-----------------------------------------------*/
    if (!strcmp(cmd, "detach")) {
        int err = bpf_xdp_detach(ifindex, xdp_flags, NULL);
        if (err)
            die("bpf_xdp_detach failed: %s", strerror(-err));
        printf("detached XDP from ifname=%s mode=%s\n", ifname, mode);
        return EXIT_SUCCESS;
    }

    /* 必须指定 "run" */
    if (strcmp(cmd, "run") != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* 解析 action 参数 */
    if (parse_action(action_str, &action) != 0)
        die("invalid --action: %s", action_str);

    /* 参数默认值 */
    if (duration <= 0) duration = 10;
    if (interval <= 0) interval = 1;

    /*-----------------------------------------------
     * 安装信号处理器（支持 Ctrl+C 优雅退出）
     *-----------------------------------------------*/
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    /* 解除内存锁限制（BPF 加载需要）*/
    bump_memlock_rlimit();

    /*-----------------------------------------------
     * libbpf 加载 BPF object
     *
     * 步骤：
     *   1. bpf_object__open_file     — 打开 .bpf.o，解析 ELF
     *   2. bpf_object__load          — 分配 map，加载 program 到内核
     *   3. bpf_object__find_program_by_name — 找到 SEC("xdp") 程序
     *   4. bpf_program__fd            — 获取 program fd
     *-----------------------------------------------*/

    struct bpf_object *obj = bpf_object__open_file(obj_path, NULL);
    if (!obj)
        die("bpf_object__open_file(%s) failed", obj_path);

    int err = bpf_object__load(obj);
    if (err)
        die("bpf_object__load failed: %s", strerror(-err));

    /* 找到 "xdp_redirect_basics" 这个 SEC("xdp") 程序 */
    struct bpf_program *prog = bpf_object__find_program_by_name(obj, "xdp_redirect_basics");
    if (!prog)
        die("find xdp_redirect_basics program failed");

    int prog_fd = bpf_program__fd(prog);
    if (prog_fd < 0)
        die("bpf_program__fd failed");

    /*-----------------------------------------------
     * 找到 config_map 和 stats_map
     * 用户态通过 config_map 控制 action，
     * 通过 stats_map 读取统计。
     *-----------------------------------------------*/
    struct bpf_map *config_map = bpf_object__find_map_by_name(obj, "config_map");
    struct bpf_map *stats_map = bpf_object__find_map_by_name(obj, "stats_map");
    if (!config_map || !stats_map)
        die("find required maps failed");

    int config_fd = bpf_map__fd(config_map);
    int stats_fd = bpf_map__fd(stats_map);
    if (config_fd < 0 || stats_fd < 0)
        die("map fd invalid");

    /*-----------------------------------------------
     * 向 config_map[0] 写入用户指定的 action
     * BPF 程序运行时会查这个值决定行为
     *-----------------------------------------------*/
    uint32_t key = 0;
    if (bpf_map_update_elem(config_fd, &key, &action, BPF_ANY) != 0)
        die("update config_map failed: %s", strerror(errno));

    /*-----------------------------------------------
     * attach XDP program 到网卡
     *
     * bpf_xdp_attach 是 libbpf 1.0+ API，
     * 内部调用 netlink 将 program fd 注册到网卡的 XDP hook。
     * 之后网卡收到的包都会先经过这个 BPF 程序。
     *-----------------------------------------------*/
    err = bpf_xdp_attach(ifindex, prog_fd, xdp_flags, NULL);
    if (err)
        die("bpf_xdp_attach ifname=%s mode=%s failed: %s", ifname, mode, strerror(-err));

    /* 获取 CPU 核数（用于读取 per-CPU stats_map）*/
    int ncpus = libbpf_num_possible_cpus();
    if (ncpus <= 0)
        ncpus = 1;

    /* 打印启动信息 */
    printf("attached XDP program\n");
    printf("ifname=%s ifindex=%d mode=%s action=%s(%u) duration=%d interval=%d obj=%s ncpus=%d\n",
           ifname, ifindex, mode, action_name(action), action, duration, interval, obj_path, ncpus);
    fflush(stdout);

    /*-----------------------------------------------
     * 主循环：每 interval 秒打印一次统计
     * 按 duration 控制总运行时长
     *-----------------------------------------------*/
    for (int elapsed = 0; !stop && elapsed < duration; elapsed += interval) {
        sleep((unsigned int)interval);
        print_stats(stats_fd, ncpus);
    }

    /*-----------------------------------------------
     * 清理：detach XDP program
     *-----------------------------------------------*/
    printf("detaching XDP program\n");
    err = bpf_xdp_detach(ifindex, xdp_flags, NULL);
    if (err)
        fprintf(stderr, "WARN: detach failed: %s\n", strerror(-err));
    else
        printf("detach ok\n");

    /* 关闭 bpf_object，释放资源 */
    bpf_object__close(obj);
    return EXIT_SUCCESS;
}
