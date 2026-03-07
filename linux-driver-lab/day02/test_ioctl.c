#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "demo_ioctl.h" // 必须包含这个头文件，否则找不到命令号

int main() {
    int fd;
    int val_to_set = 88;
    int val_to_get = 0;

    // 1. 打开设备节点
    fd = open("/dev/demo", O_RDWR);
    if (fd < 0) {
        perror("❌ 无法打开 /dev/demo");
        return -1;
    }

    printf("--- Day 02 IOCTL 测试开始 ---\n");

    // 2. 测试 SET 操作
    printf("1. 正在尝试设置内核变量值为: %d\n", val_to_set);
    if (ioctl(fd, DEMO_SET_VAL, &val_to_set) < 0) {
        perror("   SET 失败");
    } else {
        printf("   SET 成功！\n");
    }

    // 3. 测试 GET 操作
    if (ioctl(fd, DEMO_GET_VAL, &val_to_get) < 0) {
        perror("   GET 失败");
    } else {
        printf("2. 从内核读取到的值是: %d\n", val_to_get);
    }

    // 4. 测试错误码规范 (传入一个无效的命令)
    printf("3. 正在测试无效命令 (应返回 EINVAL)...\n");
    if (ioctl(fd, _IO('x', 99), NULL) < 0) {
        printf("   预期内的错误！错误码: %d (EINVAL=%d)\n", errno, EINVAL);
    }

    // 5. 测试错误码规范 (传入非法内存地址)
    printf("4. 正在测试非法地址 (应返回 EFAULT)...\n");
    if (ioctl(fd, DEMO_SET_VAL, NULL) < 0) {
        printf("   预期内的错误！错误码: %d (EFAULT=%d)\n", errno, EFAULT);
    }

    printf("--- 测试结束 ---\n");

    close(fd);
    return 0;
}
