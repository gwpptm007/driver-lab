/*
 * Day26 userspace tool
 *
 * 设计目标：
 * 1. 把 Day26 驱动暴露出来的 read/write/ioctl 接口做成一个简单好记的命令行工具；
 * 2. 让 guest 自动流程和人工调试都可以直接复用这同一份工具；
 * 3. 保持错误码清晰：open 失败、ioctl 失败、write 失败分别对应不同返回码。
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../include/day26_edu_uapi.h"

/*
 * 这些返回码会被 guest/init.day26 记录到 records/ 中。
 * 尤其是 trigger 0 的负向测试，期望走到 DAY26_RC_WRITE=5。
 */
enum {
    DAY26_RC_OK = 0,
    DAY26_RC_OPEN = 1,
    DAY26_RC_USAGE = 2,
    DAY26_RC_IOCTL = 3,
    DAY26_RC_READ = 4,
    DAY26_RC_WRITE = 5,
};

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s <dev> info\n"
            "  %s <dev> read-state\n"
            "  %s <dev> trigger <hex_or_dec_value>\n"
            "  %s <dev> count\n"
            "  %s <dev> status\n"
            "  %s <dev> reset-stats\n",
            prog, prog, prog, prog, prog, prog);
}

/*
 * 统一解析一个 u32：既支持 1，也支持 0x1。
 * 无效输入直接退出为 DAY26_RC_USAGE。
 */
static unsigned parse_u32(const char *s)
{
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 0);
    if (!s[0] || (end && *end)) {
        fprintf(stderr, "invalid integer: %s\n", s);
        exit(DAY26_RC_USAGE);
    }
    if (v > 0xffffffffUL) {
        fprintf(stderr, "value too large: %s\n", s);
        exit(DAY26_RC_USAGE);
    }
    return (unsigned)v;
}

int main(int argc, char **argv)
{
    int fd;

    if (argc < 3) {
        usage(argv[0]);
        return DAY26_RC_USAGE;
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", argv[1], strerror(errno));
        return DAY26_RC_OPEN;
    }

    if (strcmp(argv[2], "info") == 0) {
        /* 结构化状态：来自 ioctl(DAY26_IOC_GET_INFO)。 */
        struct day26_info info;
        if (ioctl(fd, DAY26_IOC_GET_INFO, &info) != 0) {
            fprintf(stderr, "DAY26_IOC_GET_INFO failed: %s\n", strerror(errno));
            close(fd);
            return DAY26_RC_IOCTL;
        }
        printf("tool_api_version=%u\n", info.tool_api_version);
        printf("vendor=0x%04x device=0x%04x\n", info.vendor_id, info.device_id);
        printf("bar0_start=0x%llx bar0_len=0x%llx\n",
               (unsigned long long)info.bar0_start,
               (unsigned long long)info.bar0_len);
        printf("irq_vector=%u irq_count=%llu msi_enabled=%u\n",
               info.irq_vector,
               (unsigned long long)info.irq_count,
               info.msi_enabled);
        printf("identity_value=0x%08x\n", info.identity_value);
        printf("liveness_value=0x%08x liveness_inverted=0x%08x\n",
               info.liveness_value, info.liveness_inverted);
        printf("last_irq_status=0x%08x last_ack_value=0x%08x\n",
               info.last_irq_status, info.last_ack_value);
    } else if (strcmp(argv[2], "read-state") == 0) {
        /* 文本状态：来自驱动 read()。 */
        char buf[512];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n < 0) {
            fprintf(stderr, "read-state failed: %s\n", strerror(errno));
            close(fd);
            return DAY26_RC_READ;
        }
        buf[n] = '\0';
        printf("%s", buf);
    } else if (strcmp(argv[2], "trigger") == 0) {
        /*
         * 触发路径：用户态往 write() 写一个十进制/十六进制整数。
         * 例如：trigger 0x1
         * 负向测试：trigger 0 -> 驱动返回 -EINVAL，工具返回 DAY26_RC_WRITE。
         */
        char kbuf[32];
        unsigned value;
        ssize_t n;
        if (argc != 4) {
            usage(argv[0]);
            close(fd);
            return DAY26_RC_USAGE;
        }
        value = parse_u32(argv[3]);
        snprintf(kbuf, sizeof(kbuf), "0x%x\n", value);
        n = write(fd, kbuf, strlen(kbuf));
        if (n < 0) {
            fprintf(stderr, "write trigger failed: %s\n", strerror(errno));
            close(fd);
            return DAY26_RC_WRITE;
        }
        printf("triggered value=0x%08x\n", value);
    } else if (strcmp(argv[2], "count") == 0) {
        struct day26_irq_count cnt;
        if (ioctl(fd, DAY26_IOC_GET_IRQ_COUNT, &cnt) != 0) {
            fprintf(stderr, "DAY26_IOC_GET_IRQ_COUNT failed: %s\n", strerror(errno));
            close(fd);
            return DAY26_RC_IOCTL;
        }
        printf("irq_count=%llu\n", (unsigned long long)cnt.count);
    } else if (strcmp(argv[2], "status") == 0) {
        struct day26_irq_status st;
        if (ioctl(fd, DAY26_IOC_GET_IRQ_STATUS, &st) != 0) {
            fprintf(stderr, "DAY26_IOC_GET_IRQ_STATUS failed: %s\n", strerror(errno));
            close(fd);
            return DAY26_RC_IOCTL;
        }
        printf("irq_status=0x%08x ack_value=0x%08x\n", st.irq_status, st.ack_value);
    } else if (strcmp(argv[2], "reset-stats") == 0) {
        if (ioctl(fd, DAY26_IOC_RESET_STATS) != 0) {
            fprintf(stderr, "DAY26_IOC_RESET_STATS failed: %s\n", strerror(errno));
            close(fd);
            return DAY26_RC_IOCTL;
        }
        printf("stats reset\n");
    } else {
        usage(argv[0]);
        close(fd);
        return DAY26_RC_USAGE;
    }

    close(fd);
    return DAY26_RC_OK;
}
