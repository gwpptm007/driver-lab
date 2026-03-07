#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include "demo_ioctl.h" // 确保包含的是当前目录下的头文件

int main() {
    int fd;
    int val = 88;
    int get_val = 0;

    fd = open("/dev/demo", O_RDWR);
    if (fd < 0) {
        perror("❌ 无法打开 /dev/demo0");
        return -1;
    }

    printf("--- Day 03 Sysfs & IOCTL 测试 ---\n");

    // 1. 测试 SET
    if (ioctl(fd, DEMO_IOCTL_SET, &val) < 0) {
        printf("   SET 失败: %s\n", strerror(errno));
    } else {
        printf("   SET 成功: 发送了 %d\n", val);
    }

    // 2. 测试 GET
    if (ioctl(fd, DEMO_IOCTL_GET, &get_val) < 0) {
        printf("   GET 失败: %s\n", strerror(errno));
    } else {
        printf("   GET 成功: 收到了 %d\n", get_val);
    }

    close(fd);
    printf("--- 测试结束 ---\n");
    return 0;
}
