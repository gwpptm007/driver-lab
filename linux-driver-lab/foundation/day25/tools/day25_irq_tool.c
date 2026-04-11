#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../include/day25_edu_uapi.h"

/*
 * day25 用户态工具只做四类动作：
 * - info   : 获取静态设备信息 + 当前中断状态
 * - trigger: 写 EDU IRQ_RAISE，主动触发一次中断
 * - count  : 获取驱动内部 irq_count
 * - status : 获取最近一次中断的 status / ack 值
 */
static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s <dev> info\n"
            "  %s <dev> trigger <hex_or_dec_value>\n"
            "  %s <dev> count\n"
            "  %s <dev> status\n",
            prog, prog, prog, prog);
}

/* 支持十六进制或十进制输入，例如 0x1 / 1 */
static unsigned parse_u32(const char *s)
{
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 0);
    if (!s[0] || (end && *end)) {
        fprintf(stderr, "invalid integer: %s\n", s);
        exit(2);
    }
    if (v > 0xffffffffUL) {
        fprintf(stderr, "value too large: %s\n", s);
        exit(2);
    }
    return (unsigned)v;
}

int main(int argc, char **argv)
{
    int fd;

    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", argv[1], strerror(errno));
        return 1;
    }

    if (strcmp(argv[2], "info") == 0) {
        struct day25_info info;
        if (ioctl(fd, DAY25_IOC_GET_INFO, &info) != 0) {
            fprintf(stderr, "DAY25_IOC_GET_INFO failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        printf("vendor=0x%04x device=0x%04x\n", info.vendor_id, info.device_id);
        printf("bar0_start=0x%llx bar0_len=0x%llx\n",
               (unsigned long long)info.bar0_start,
               (unsigned long long)info.bar0_len);
        printf("irq_vector=%u irq_count=%llu msi_enabled=%u\n",
               info.irq_vector,
               (unsigned long long)info.irq_count,
               info.msi_enabled);
        printf("last_irq_status=0x%08x last_ack_value=0x%08x\n",
               info.last_irq_status, info.last_ack_value);
        printf("liveness_value=0x%08x liveness_inverted=0x%08x\n",
               info.liveness_value, info.liveness_inverted);
    } else if (strcmp(argv[2], "trigger") == 0) {
        struct day25_trigger trig;
        if (argc != 4) {
            usage(argv[0]);
            close(fd);
            return 2;
        }
        trig.value = parse_u32(argv[3]);
        if (ioctl(fd, DAY25_IOC_TRIGGER_IRQ, &trig) != 0) {
            fprintf(stderr, "DAY25_IOC_TRIGGER_IRQ failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        printf("triggered value=0x%08x\n", trig.value);
    } else if (strcmp(argv[2], "count") == 0) {
        struct day25_irq_count cnt;
        if (ioctl(fd, DAY25_IOC_GET_IRQ_COUNT, &cnt) != 0) {
            fprintf(stderr, "DAY25_IOC_GET_IRQ_COUNT failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        printf("irq_count=%llu\n", (unsigned long long)cnt.count);
    } else if (strcmp(argv[2], "status") == 0) {
        struct day25_irq_status st;
        if (ioctl(fd, DAY25_IOC_GET_IRQ_STATUS, &st) != 0) {
            fprintf(stderr, "DAY25_IOC_GET_IRQ_STATUS failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        printf("irq_status=0x%08x ack_value=0x%08x\n", st.irq_status, st.ack_value);
    } else {
        usage(argv[0]);
        close(fd);
        return 2;
    }

    close(fd);
    return 0;
}
