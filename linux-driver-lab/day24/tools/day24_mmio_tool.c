#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/day24_ivshmem_uapi.h"

#define DAY24_DEFAULT_DEV "/dev/day24_ivshmem0"

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s info [dev]\n"
            "  %s mmio-read <offset> [dev]\n"
            "  %s mmio-write <offset> <value> [dev]\n"
            "  %s shm-write <text> [dev]\n"
            "  %s shm-read [dev]\n"
            "  %s clear [dev]\n",
            prog, prog, prog, prog, prog, prog);
}

static uint32_t parse_u32(const char *s)
{
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 0);

    if (!s[0] || (end && *end)) {
        fprintf(stderr, "invalid integer: %s\n", s);
        exit(2);
    }
    if (v > 0xffffffffUL) {
        fprintf(stderr, "integer out of range: %s\n", s);
        exit(2);
    }
    return (uint32_t)v;
}

static int open_dev(const char *path)
{
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        perror(path);
        return -1;
    }
    return fd;
}

static int do_info(int fd)
{
    struct day24_info_uapi info;

    memset(&info, 0, sizeof(info));
    if (ioctl(fd, DAY24_IOC_GET_INFO, &info) < 0) {
        perror("DAY24_IOC_GET_INFO");
        return 1;
    }

    printf("vendor=0x%04x device=0x%04x\n", info.vendor, info.device);
    printf("bar0_first_dword=0x%08x\n", info.bar0_first_dword);
    printf("proto_magic=0x%08x proto_version=%u seq=%u state=%u payload_len=%u\n",
           info.proto_magic, info.proto_version, info.proto_seq,
           info.proto_state, info.proto_payload_len);
    printf("BAR0 start=0x%016llx end=0x%016llx len=0x%016llx flags=0x%016llx\n",
           (unsigned long long)info.bar0.start,
           (unsigned long long)info.bar0.end,
           (unsigned long long)info.bar0.len,
           (unsigned long long)info.bar0.flags);
    printf("BAR2 start=0x%016llx end=0x%016llx len=0x%016llx flags=0x%016llx\n",
           (unsigned long long)info.bar2.start,
           (unsigned long long)info.bar2.end,
           (unsigned long long)info.bar2.len,
           (unsigned long long)info.bar2.flags);
    return 0;
}

static int do_mmio_read(int fd, uint32_t off)
{
    struct day24_mmio32_uapi mmio;
    mmio.offset = off;
    mmio.value = 0;

    if (ioctl(fd, DAY24_IOC_MMIO_READ32, &mmio) < 0) {
        perror("DAY24_IOC_MMIO_READ32");
        return 1;
    }

    printf("mmio-read ok: offset=0x%08x value=0x%08x\n", mmio.offset, mmio.value);
    return 0;
}

static int do_mmio_write(int fd, uint32_t off, uint32_t val)
{
    struct day24_mmio32_uapi mmio;
    mmio.offset = off;
    mmio.value = val;

    if (ioctl(fd, DAY24_IOC_MMIO_WRITE32, &mmio) < 0) {
        perror("DAY24_IOC_MMIO_WRITE32");
        return 1;
    }

    printf("mmio-write ok: offset=0x%08x value=0x%08x\n", off, val);
    return 0;
}

static int do_shm_write(int fd, const char *text)
{
    ssize_t n;
    size_t len = strlen(text);

    if (lseek(fd, 0, SEEK_SET) < 0) {
        perror("lseek");
        return 1;
    }

    n = write(fd, text, len);
    if (n < 0) {
        perror("write");
        return 1;
    }

    printf("shm-write ok: wrote=%zd text=%s\n", n, text);
    return 0;
}

static int do_shm_read(int fd)
{
    char buf[DAY24_PROTO_MAX_PAYLOAD + 1];
    ssize_t n;

    if (lseek(fd, 0, SEEK_SET) < 0) {
        perror("lseek");
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, DAY24_PROTO_MAX_PAYLOAD);
    if (n < 0) {
        perror("read");
        return 1;
    }

    buf[(n >= 0 && n < (ssize_t)sizeof(buf)) ? n : (ssize_t)sizeof(buf) - 1] = '\0';
    printf("shm-read ok: read=%zd text=%s\n", n, buf);
    return 0;
}

static int do_clear(int fd)
{
    if (ioctl(fd, DAY24_IOC_CLEAR_PAYLOAD) < 0) {
        perror("DAY24_IOC_CLEAR_PAYLOAD");
        return 1;
    }
    printf("clear ok\n");
    return 0;
}

int main(int argc, char **argv)
{
    const char *cmd;
    const char *dev = DAY24_DEFAULT_DEV;
    int fd;
    int rc = 1;

    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    cmd = argv[1];

    if (!strcmp(cmd, "info")) {
        if (argc >= 3)
            dev = argv[2];
        fd = open_dev(dev);
        if (fd < 0)
            return 1;
        rc = do_info(fd);
        close(fd);
        return rc;
    }

    if (!strcmp(cmd, "mmio-read")) {
        uint32_t off;
        if (argc < 3) {
            usage(argv[0]);
            return 2;
        }
        off = parse_u32(argv[2]);
        if (argc >= 4)
            dev = argv[3];
        fd = open_dev(dev);
        if (fd < 0)
            return 1;
        rc = do_mmio_read(fd, off);
        close(fd);
        return rc;
    }

    if (!strcmp(cmd, "mmio-write")) {
        uint32_t off, val;
        if (argc < 4) {
            usage(argv[0]);
            return 2;
        }
        off = parse_u32(argv[2]);
        val = parse_u32(argv[3]);
        if (argc >= 5)
            dev = argv[4];
        fd = open_dev(dev);
        if (fd < 0)
            return 1;
        rc = do_mmio_write(fd, off, val);
        close(fd);
        return rc;
    }

    if (!strcmp(cmd, "shm-write")) {
        if (argc < 3) {
            usage(argv[0]);
            return 2;
        }
        if (argc >= 4)
            dev = argv[3];
        fd = open_dev(dev);
        if (fd < 0)
            return 1;
        rc = do_shm_write(fd, argv[2]);
        close(fd);
        return rc;
    }

    if (!strcmp(cmd, "shm-read")) {
        if (argc >= 3)
            dev = argv[2];
        fd = open_dev(dev);
        if (fd < 0)
            return 1;
        rc = do_shm_read(fd);
        close(fd);
        return rc;
    }

    if (!strcmp(cmd, "clear")) {
        if (argc >= 3)
            dev = argv[2];
        fd = open_dev(dev);
        if (fd < 0)
            return 1;
        rc = do_clear(fd);
        close(fd);
        return rc;
    }

    usage(argv[0]);
    return 2;
}
