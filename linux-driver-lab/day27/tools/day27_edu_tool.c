// SPDX-License-Identifier: GPL-2.0
/*
 * Day27 userspace tool
 *
 * 角色很简单：
 * 1. info/count/reset 走 ioctl，方便脚本做结构化校验；
 * 2. read-state 走 read() 文本接口，便于直接落盘归档；
 * 3. trigger 走 write()，验证“用户态写入 -> 设备 raise IRQ -> 内核 handler”这条路径。
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../include/day27_edu_uapi.h"

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s <dev> info\n"
            "  %s <dev> read-state\n"
            "  %s <dev> count\n"
            "  %s <dev> trigger <value>\n"
            "  %s <dev> reset-stats\n",
            prog, prog, prog, prog, prog);
}

static int do_info(int fd)
{
    struct day27_info info;
    if (ioctl(fd, DAY27_IOC_GET_INFO, &info) != 0) {
        perror("ioctl(GET_INFO)");
        return 1;
    }
    printf("vendor=0x%04x device=0x%04x irq_vector=%u irq_count=%llu msi_enabled=%u\n",
           info.vendor_id, info.device_id, info.irq_vector,
           (unsigned long long)info.irq_count, info.msi_enabled);
    printf("bar0_start=0x%llx bar0_len=0x%llx last_irq_status=0x%08x last_ack_value=0x%08x\n",
           (unsigned long long)info.bar0_start,
           (unsigned long long)info.bar0_len,
           info.last_irq_status,
           info.last_ack_value);
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

static int do_count(int fd)
{
    struct day27_irq_count cnt;
    if (ioctl(fd, DAY27_IOC_GET_IRQ_COUNT, &cnt) != 0) {
        perror("ioctl(GET_IRQ_COUNT)");
        return 1;
    }
    printf("irq_count=%llu\n", (unsigned long long)cnt.count);
    return 0;
}

static int do_trigger(int fd, const char *arg)
{
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%s\n", arg);
    ssize_t wr = write(fd, buf, (size_t)len);
    if (wr < 0) {
        fprintf(stderr, "write trigger failed: %s\n", strerror(errno));
        return 1;
    }
    printf("triggered value=%s\n", arg);
    return 0;
}

static int do_reset_stats(int fd)
{
    if (ioctl(fd, DAY27_IOC_RESET_STATS) != 0) {
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
    else if (strcmp(argv[2], "count") == 0)
        rc = do_count(fd);
    else if (strcmp(argv[2], "trigger") == 0) {
        if (argc < 4) {
            usage(argv[0]);
            rc = 2;
        } else {
            rc = do_trigger(fd, argv[3]);
        }
    } else if (strcmp(argv[2], "reset-stats") == 0)
        rc = do_reset_stats(fd);
    else {
        usage(argv[0]);
        rc = 2;
    }

    close(fd);
    return rc;
}
