// SPDX-License-Identifier: GPL-2.0
/*
 * Day34 guest stability tool
 *
 * 工具层不再关注 perf/trace，而是提供：
 * 1. 冒烟验证 `mmap-verify`
 * 2. 并发 worker：`stress-mmap` / `stress-ioctl`
 * 3. 错误注入：非法长度 / 非法页偏移
 */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "day34_edu_uapi.h"

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s <dev> info\n"
        "  %s <dev> read-state\n"
        "  %s <dev> mmap-verify <len> <seed>\n"
        "  %s <dev> stress-mmap <len> <iterations> <seed_base>\n"
        "  %s <dev> stress-ioctl <iterations>\n"
        "  %s <dev> fault-invalid-len <len> <seed>\n"
        "  %s <dev> fault-mmap-offset <pgoff>\n"
        "  %s <dev> result\n"
        "  %s <dev> reset-stats\n",
        prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

static int get_info(int fd, struct day34_info *info)
{
    if (ioctl(fd, DAY34_IOC_GET_INFO, info) != 0) {
        perror("ioctl(GET_INFO)");
        return -1;
    }
    return 0;
}

static int get_result(int fd, struct day34_run_result *res)
{
    if (ioctl(fd, DAY34_IOC_GET_RESULT, res) != 0) {
        perror("ioctl(GET_RESULT)");
        return -1;
    }
    return 0;
}

static void fill_pattern(uint8_t *buf, uint32_t len, uint32_t seed)
{
    uint32_t i;
    for (i = 0; i < len; ++i)
        buf[i] = (uint8_t)((seed + i) & 0xff);
}

static int prepare_mapping(int fd, uint32_t len, struct day34_info *info,
                           uint8_t **map_out, uint8_t **src_out, uint8_t **dst_out)
{
    uint8_t *map;

    if (get_info(fd, info) != 0)
        return -1;
    if (!len || len > info->max_verify_len) {
        fprintf(stderr, "invalid len %u, max_verify_len=%u\n", len, info->max_verify_len);
        return -1;
    }

    map = mmap(NULL, info->map_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        return -1;
    }

    *map_out = map;
    *src_out = map + info->src_off;
    *dst_out = map + info->dst_off;
    return 0;
}

static int cmd_info(int fd)
{
    struct day34_info info;
    if (get_info(fd, &info) != 0)
        return 1;
    printf("tool_api_version=%u\n", info.tool_api_version);
    printf("vendor_id=0x%04x\n", info.vendor_id);
    printf("device_id=0x%04x\n", info.device_id);
    printf("irq_vector=%u\n", info.irq_vector);
    printf("irq_count=%llu\n", (unsigned long long)info.irq_count);
    printf("dma_handle=0x%llx\n", (unsigned long long)info.dma_handle);
    printf("dma_bytes=%u\n", info.dma_bytes);
    printf("dma_mask_bits=%u\n", info.dma_mask_bits);
    printf("map_bytes=%u\n", info.map_bytes);
    printf("src_off=%u\n", info.src_off);
    printf("dst_off=%u\n", info.dst_off);
    printf("max_verify_len=%u\n", info.max_verify_len);
    return 0;
}

static int cmd_read_state(int fd)
{
    char buf[1024];
    ssize_t n = pread(fd, buf, sizeof(buf) - 1, 0);
    if (n < 0) {
        perror("read-state");
        return 1;
    }
    buf[n] = '\0';
    fputs(buf, stdout);
    return 0;
}

static int cmd_mmap_verify(int fd, uint32_t len, uint32_t seed)
{
    struct day34_info info;
    struct day34_run_req req;
    struct day34_run_result res;
    uint8_t *map = NULL, *src = NULL, *dst = NULL;
    int rc = 1;
    int mismatch = -1;
    uint32_t i;

    if (prepare_mapping(fd, len, &info, &map, &src, &dst) != 0)
        return 1;

    fill_pattern(src, len, seed);
    memset(dst, 0, len);
    req.len = len;
    req.pattern_seed = seed;
    if (ioctl(fd, DAY34_IOC_RUN_DMA, &req) != 0) {
        perror("ioctl(RUN_DMA)");
        goto out;
    }
    if (get_result(fd, &res) != 0)
        goto out;

    for (i = 0; i < len; ++i) {
        if (src[i] != dst[i]) {
            mismatch = (int)i;
            break;
        }
    }

    printf("mmap_ok=1\n");
    printf("verify_len=%u\n", len);
    printf("verify_seed=0x%x\n", seed);
    printf("run_ok=%u\n", res.run_ok);
    printf("run_error=%d\n", res.run_error);
    printf("irq_delta=%u\n", res.irq_delta);
    printf("verify_ok=%u\n", mismatch < 0 ? 1U : 0U);
    printf("mismatch_index=%d\n", mismatch);
    rc = 0;
out:
    if (map)
        munmap(map, info.map_bytes);
    return rc;
}

static int cmd_stress_mmap(int fd, uint32_t len, uint32_t iterations, uint32_t seed_base)
{
    struct day34_info info;
    struct day34_run_req req;
    uint8_t *map = NULL, *src = NULL, *dst = NULL;
    uint32_t ok = 0, fail = 0;
    uint32_t i, j;
    int rc = 1;

    if (!iterations) {
        fprintf(stderr, "iterations must be > 0\n");
        return 1;
    }
    if (prepare_mapping(fd, len, &info, &map, &src, &dst) != 0)
        return 1;

    for (i = 0; i < iterations; ++i) {
        uint32_t seed = seed_base + i;
        /*
         * 所有 worker 共用同一个 DMA coherent buffer。为了把 day34 的“并发”
         * 聚焦在多进程争用设备与 ioctl/mmap 入口，而不是把共享 src/dst 缓冲
         * 区本身写乱，这里在“填充 -> RUN_DMA -> compare”这一整段外侧加
         * flock()。
         *
         * 这样多个进程仍会并发创建、竞争锁、反复进入驱动，但同一时刻只有
         * 一个 worker 会操作共享的 src/dst 半页，避免把数据竞争误报成驱动
         * 不稳定。
         */
        if (flock(fd, LOCK_EX) != 0) {
            fail++;
            continue;
        }
        fill_pattern(src, len, seed);
        memset(dst, 0, len);
        req.len = len;
        req.pattern_seed = seed;
        if (ioctl(fd, DAY34_IOC_RUN_DMA, &req) != 0) {
            fail++;
            flock(fd, LOCK_UN);
            continue;
        }
        for (j = 0; j < len; ++j) {
            if (src[j] != dst[j]) {
                fail++;
                flock(fd, LOCK_UN);
                goto next_iter;
            }
        }
        ok++;
        flock(fd, LOCK_UN);
next_iter:
        ;
    }

    printf("mode=stress-mmap\n");
    printf("iterations=%u\n", iterations);
    printf("success_ops=%u\n", ok);
    printf("failed_ops=%u\n", fail);
    printf("success_rate=%.2f\n", iterations ? ((double)ok * 100.0 / (double)iterations) : 0.0);
    rc = (fail == 0) ? 0 : 2;
    munmap(map, info.map_bytes);
    return rc;
}

static int cmd_stress_ioctl(int fd, uint32_t iterations)
{
    struct day34_info info;
    struct day34_run_result res;
    uint32_t ok = 0, fail = 0, i;

    if (!iterations) {
        fprintf(stderr, "iterations must be > 0\n");
        return 1;
    }

    for (i = 0; i < iterations; ++i) {
        if (get_info(fd, &info) != 0) {
            fail++;
            continue;
        }
        if (get_result(fd, &res) != 0) {
            fail++;
            continue;
        }
        ok++;
    }
    printf("mode=stress-ioctl\n");
    printf("iterations=%u\n", iterations);
    printf("success_ops=%u\n", ok);
    printf("failed_ops=%u\n", fail);
    printf("success_rate=%.2f\n", iterations ? ((double)ok * 100.0 / (double)iterations) : 0.0);
    return fail == 0 ? 0 : 2;
}

static int cmd_fault_invalid_len(int fd, uint32_t len, uint32_t seed)
{
    struct day34_run_req req;
    req.len = len;
    req.pattern_seed = seed;
    errno = 0;
    if (ioctl(fd, DAY34_IOC_RUN_DMA, &req) == 0) {
        printf("expected_failure=0\n");
        printf("unexpected_success=1\n");
        return 2;
    }
    printf("expected_failure=1\n");
    printf("unexpected_success=0\n");
    printf("errno=%d\n", errno);
    printf("error_text=%s\n", strerror(errno));
    return 0;
}

/*
 * fault-mmap-offset 用来验证字符设备 mmap 的页偏移边界检查是否生效。
 * Day34 驱动只允许 pgoff=0 的整页映射；这里故意传入非法页偏移，期待
 * 驱动返回 EINVAL，并把最近一次 mmap 结果记录到 result 快照里。
 */
static int cmd_fault_mmap_offset(int fd, uint32_t pgoff)
{
    struct day34_info info;
    void *map;
    if (get_info(fd, &info) != 0)
        return 1;
    errno = 0;
    map = mmap(NULL, info.map_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)pgoff << 12);
    if (map == MAP_FAILED) {
        printf("expected_failure=1\n");
        printf("unexpected_success=0\n");
        printf("errno=%d\n", errno);
        printf("error_text=%s\n", strerror(errno));
        return 0;
    }
    munmap(map, info.map_bytes);
    printf("expected_failure=0\n");
    printf("unexpected_success=1\n");
    return 2;
}

/*
 * result 读取的是驱动里“最近一次操作”的状态快照，不是 Day34 整轮
 * 稳定性测试的汇总。当前自动化把两条 fault 注入放在 result 之前执行，
 * 所以 run-result.txt 更适合解释最近一次 fault 是否被驱动正确记录。
 */
static int cmd_result(int fd)
{
    struct day34_run_result res;
    if (get_result(fd, &res) != 0)
        return 1;
    printf("total_run_calls=%llu\n", (unsigned long long)res.total_run_calls);
    printf("total_run_ok=%llu\n", (unsigned long long)res.total_run_ok);
    printf("total_run_fail=%llu\n", (unsigned long long)res.total_run_fail);
    printf("last_run_ns=%llu\n", (unsigned long long)res.last_run_ns);
    printf("run_len=%u\n", res.run_len);
    printf("run_seed=0x%x\n", res.run_seed);
    printf("run_ok=%u\n", res.run_ok);
    printf("run_error=%d\n", res.run_error);
    printf("irq_delta=%u\n", res.irq_delta);
    printf("last_dma_cmd=0x%x\n", res.last_dma_cmd);
    printf("mmap_ok=%u\n", res.mmap_ok);
    printf("mmap_error=%d\n", res.mmap_error);
    printf("mmap_len=%u\n", res.mmap_len);
    printf("mmap_pgoff=%u\n", res.mmap_pgoff);
    return 0;
}

static int cmd_reset_stats(int fd)
{
    if (ioctl(fd, DAY34_IOC_RESET_STATS) != 0) {
        perror("ioctl(RESET_STATS)");
        return 1;
    }
    return 0;
}

static unsigned long parse_ul(const char *s, const char *what)
{
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 0);
    if (!s[0] || (end && *end)) {
        fprintf(stderr, "invalid %s: %s\n", what, s);
        exit(2);
    }
    return v;
}

int main(int argc, char **argv)
{
    const char *dev;
    const char *cmd;
    int fd;
    int rc = 0;

    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }
    dev = argv[1];
    cmd = argv[2];
    fd = open(dev, O_RDWR);
    if (fd < 0) {
        perror(dev);
        return 1;
    }

    if (!strcmp(cmd, "info")) {
        rc = cmd_info(fd);
    } else if (!strcmp(cmd, "read-state")) {
        rc = cmd_read_state(fd);
    } else if (!strcmp(cmd, "mmap-verify")) {
        if (argc != 5) { usage(argv[0]); rc = 2; }
        else rc = cmd_mmap_verify(fd, (uint32_t)parse_ul(argv[3], "len"), (uint32_t)parse_ul(argv[4], "seed"));
    } else if (!strcmp(cmd, "stress-mmap")) {
        if (argc != 6) { usage(argv[0]); rc = 2; }
        else rc = cmd_stress_mmap(fd, (uint32_t)parse_ul(argv[3], "len"), (uint32_t)parse_ul(argv[4], "iterations"), (uint32_t)parse_ul(argv[5], "seed_base"));
    } else if (!strcmp(cmd, "stress-ioctl")) {
        if (argc != 4) { usage(argv[0]); rc = 2; }
        else rc = cmd_stress_ioctl(fd, (uint32_t)parse_ul(argv[3], "iterations"));
    } else if (!strcmp(cmd, "fault-invalid-len")) {
        if (argc != 5) { usage(argv[0]); rc = 2; }
        else rc = cmd_fault_invalid_len(fd, (uint32_t)parse_ul(argv[3], "len"), (uint32_t)parse_ul(argv[4], "seed"));
    } else if (!strcmp(cmd, "fault-mmap-offset")) {
        if (argc != 4) { usage(argv[0]); rc = 2; }
        else rc = cmd_fault_mmap_offset(fd, (uint32_t)parse_ul(argv[3], "pgoff"));
    } else if (!strcmp(cmd, "result")) {
        rc = cmd_result(fd);
    } else if (!strcmp(cmd, "reset-stats")) {
        rc = cmd_reset_stats(fd);
    } else {
        usage(argv[0]);
        rc = 2;
    }
    close(fd);
    return rc;
}
