/*
 * pci_sysfs_dump.c
 *
 * day22 的 guest 侧最小 PCI 探针工具。
 *
 * 为什么 day22 就要放这个 C 工具：
 * 1. 不能让 day22 只有 shell 脚本，否则学习体验会像“搭环境”，不像“学驱动”。
 * 2. 这个工具不依赖 libpci，可以在最小 rootfs 里工作；它直接走 sysfs，
 *    让你看清 Linux 在没有真正 pci_driver 接管设备前，仍然暴露了哪些 PCI 信息。
 * 3. day23 开始写 pci_driver 以后，你可以把这个工具的输出和驱动 probe 日志对照起来看。
 * 4. 支持通过环境变量 PCI_SYSFS_ROOT 覆盖默认路径，方便 day22 在宿主机做伪 sysfs 自测。
 *
 * 本工具做的事情：
 *   - 遍历 /sys/bus/pci/devices
 *   - 打印 BDF、vendor/device/class、IRQ、resource
 *   - 读取 config 空间前 64 字节，作为“设备确实可访问”的证据
 *
 * 编译建议：
 *   aarch64-linux-gnu-gcc -O2 -static -Wall -Wextra -o pci_sysfs_dump pci_sysfs_dump.c
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_SYSFS_PCI_DEVICES "/sys/bus/pci/devices"
#define CONFIG_PREVIEW_BYTES 64

static int read_first_line(const char *path, char *buf, size_t size)
{
    int fd;
    ssize_t n;
    size_t i;

    if (size == 0)
        return -1;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    n = read(fd, buf, size - 1);
    close(fd);
    if (n <= 0)
        return -1;

    buf[n] = '\0';
    for (i = 0; i < (size_t)n; i++) {
        if (buf[i] == '\n' || buf[i] == '\r') {
            buf[i] = '\0';
            break;
        }
    }
    return 0;
}

static void dump_file_if_exists(const char *label, const char *path)
{
    char line[256];

    if (read_first_line(path, line, sizeof(line)) == 0)
        printf("  %-8s : %s\n", label, line);
}

static void dump_resource_table(const char *path)
{
    FILE *fp;
    char line[256];
    int row = 0;

    fp = fopen(path, "r");
    if (!fp)
        return;

    puts("  resource :");
    while (fgets(line, sizeof(line), fp)) {
        printf("    [%d] %s", row++, line);
        if (strchr(line, '\n') == NULL)
            putchar('\n');
    }
    fclose(fp);
}

static void dump_config_preview(const char *path)
{
    int fd;
    unsigned char buf[CONFIG_PREVIEW_BYTES];
    ssize_t n;
    size_t i;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("  config   : <open failed: %s>\n", strerror(errno));
        return;
    }

    n = read(fd, buf, sizeof(buf));
    close(fd);
    if (n <= 0) {
        printf("  config   : <read failed: %s>\n", strerror(errno));
        return;
    }

    puts("  config   :");
    for (i = 0; i < (size_t)n; i++) {
        if (i % 16 == 0)
            printf("    %02zx:", i);
        printf(" %02x", buf[i]);
        if (i % 16 == 15 || i + 1 == (size_t)n)
            putchar('\n');
    }
}

static void dump_one_device(const char *sysfs_root, const char *bdf)
{
    char path[PATH_MAX];

    printf("[device] %s\n", bdf);

    snprintf(path, sizeof(path), "%s/%s/vendor", sysfs_root, bdf);
    dump_file_if_exists("vendor", path);

    snprintf(path, sizeof(path), "%s/%s/device", sysfs_root, bdf);
    dump_file_if_exists("device", path);

    snprintf(path, sizeof(path), "%s/%s/class", sysfs_root, bdf);
    dump_file_if_exists("class", path);

    snprintf(path, sizeof(path), "%s/%s/irq", sysfs_root, bdf);
    dump_file_if_exists("irq", path);

    snprintf(path, sizeof(path), "%s/%s/resource", sysfs_root, bdf);
    dump_resource_table(path);

    snprintf(path, sizeof(path), "%s/%s/config", sysfs_root, bdf);
    dump_config_preview(path);

    putchar('\n');
}

int main(void)
{
    DIR *dir;
    struct dirent *de;
    int count = 0;
    const char *sysfs_root;

    sysfs_root = getenv("PCI_SYSFS_ROOT");
    if (!sysfs_root || !*sysfs_root)
        sysfs_root = DEFAULT_SYSFS_PCI_DEVICES;

    dir = opendir(sysfs_root);
    if (!dir) {
        fprintf(stderr, "[pci_sysfs_dump] failed to open %s: %s\n",
                sysfs_root, strerror(errno));
        return 1;
    }

    puts("# day22 pci_sysfs_dump");
    printf("# source: %s\n", sysfs_root);
    puts("# note  : this is a guest-side helper for day22 enumeration evidence");
    putchar('\n');

    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        dump_one_device(sysfs_root, de->d_name);
        count++;
    }

    closedir(dir);

    printf("# total devices: %d\n", count);
    return 0;
}
