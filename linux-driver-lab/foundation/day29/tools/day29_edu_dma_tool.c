// SPDX-License-Identifier: GPL-2.0
/*
 * Day29 guest user tool
 *
 * 这个工具故意保持得很轻：它不直接碰 MMIO，也不做任何 DMA 编程，
 * 只负责通过 ioctl 驱动字符设备，触发内核态的验证路径并把结果打印出来。
 * 这样一来，Day29 的“DMA 逻辑在驱动里，用户态只负责触发与取证”的边界就很清楚。
 */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "day29_edu_uapi.h"

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s <dev> info\n"
            "  %s <dev> read-state\n"
            "  %s <dev> verify <len> <seed>\n"
            "  %s <dev> result\n"
            "  %s <dev> reset-stats\n",
            prog, prog, prog, prog, prog);
}

/*
 * 读取驱动当前状态。
 *
 * 这里输出的是一次“快照”：既包含设备静态信息（vendor/device、BAR、MSI），
 * 也包含最近一次 verify 的结果。它非常适合写入 records/tool-info.txt。
 */
static int do_info(int fd)
{
    struct day29_info info;

    if (ioctl(fd, DAY29_IOC_GET_INFO, &info) != 0) {
        perror("ioctl(GET_INFO)");
        return 1;
    }

    printf("tool_api_version=%u\n", info.tool_api_version);
    printf("vendor=0x%04x device=0x%04x\n", info.vendor_id, info.device_id);
    printf("irq_vector=%u irq_count=%" PRIu64 " msi_enabled=%u\n",
           info.irq_vector, (uint64_t)info.irq_count, info.msi_enabled);
    printf("last_irq_status=0x%08x last_ack_value=0x%08x\n",
           info.last_irq_status, info.last_ack_value);
    printf("bar0_start=0x%" PRIx64 " bar0_len=0x%" PRIx64 "\n",
           (uint64_t)info.bar0_start, (uint64_t)info.bar0_len);
    printf("dma_handle=0x%" PRIx64 " dma_bytes=%u dma_mask_bits=%u\n",
           (uint64_t)info.dma_handle, info.dma_bytes, info.dma_mask_bits);
    printf("verify_len=%u verify_seed=0x%x verify_ok=%u verify_error=%d\n",
           info.last_verify_len, info.last_verify_seed,
           info.last_verify_ok, info.last_verify_error);
    printf("mismatch_index=%d mismatch_expected=0x%02x mismatch_actual=0x%02x\n",
           info.last_mismatch_index,
           info.last_mismatch_expected,
           info.last_mismatch_actual);
    printf("last_irq_delta=%u last_dma_cmd=0x%08x\n",
           info.last_irq_delta, info.last_dma_cmd);
    return 0;
}

static int do_read_state(int fd)
{
    char buf[512];
    ssize_t n = pread(fd, buf, sizeof(buf) - 1, 0);
    if (n < 0) {
        perror("read-state");
        return 1;
    }
    buf[n] = '\0';
    fputs(buf, stdout);
    return 0;
}

/*
 * 触发一次 round-trip verify。
 *
 * len/seed 直接透传给内核，真正的长度检查、DMA 编程和数据比对都由驱动完成。
 */
static int do_verify(int fd, const char *len_arg, const char *seed_arg)
{
    struct day29_verify_req req;
    req.len = (unsigned int)strtoul(len_arg, NULL, 0);
    req.pattern_seed = (unsigned int)strtoul(seed_arg, NULL, 0);

    if (ioctl(fd, DAY29_IOC_RUN_VERIFY, &req) != 0) {
        fprintf(stderr, "DAY29_IOC_RUN_VERIFY failed: %s\n", strerror(errno));
        return 1;
    }

    printf("verify submitted: len=%u seed=0x%x\n", req.len, req.pattern_seed);
    return 0;
}

/*
 * 读取最近一次 verify 的结构化结果。
 *
 * 返回值约定为：verify_ok=1 时退出码 0，否则退出码 1。这样 guest init 里既能
 * 打印字段，也能在需要时把它作为 shell 里的判定条件。
 */
static int do_result(int fd)
{
    struct day29_verify_result res;

    if (ioctl(fd, DAY29_IOC_GET_VERIFY_RESULT, &res) != 0) {
        perror("ioctl(GET_VERIFY_RESULT)");
        return 1;
    }

    printf("verify_len=%u\n", res.verify_len);
    printf("verify_seed=0x%x\n", res.verify_seed);
    printf("verify_ok=%u\n", res.verify_ok);
    printf("verify_error=%d\n", res.verify_error);
    printf("mismatch_index=%d\n", res.mismatch_index);
    printf("mismatch_expected=0x%02x\n", res.mismatch_expected);
    printf("mismatch_actual=0x%02x\n", res.mismatch_actual);
    printf("irq_delta=%u\n", res.irq_delta);
    printf("last_dma_cmd=0x%08x\n", res.last_dma_cmd);
    return res.verify_ok ? 0 : 1;
}

static int do_reset_stats(int fd)
{
    if (ioctl(fd, DAY29_IOC_RESET_STATS) != 0) {
        perror("ioctl(RESET_STATS)");
        return 1;
    }
    puts("reset-stats ok");
    return 0;
}

int main(int argc, char **argv)
{
    int fd, rc = 0;

    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", argv[1], strerror(errno));
        return 3;
    }

    if (strcmp(argv[2], "info") == 0)
        rc = do_info(fd);
    else if (strcmp(argv[2], "read-state") == 0)
        rc = do_read_state(fd);
    else if (strcmp(argv[2], "verify") == 0) {
        if (argc < 5) {
            usage(argv[0]);
            rc = 2;
        } else {
            rc = do_verify(fd, argv[3], argv[4]);
        }
    } else if (strcmp(argv[2], "result") == 0)
        rc = do_result(fd);
    else if (strcmp(argv[2], "reset-stats") == 0)
        rc = do_reset_stats(fd);
    else {
        usage(argv[0]);
        rc = 2;
    }

    close(fd);
    return rc;
}
