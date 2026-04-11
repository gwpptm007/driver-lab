#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DEMO_IOC_MAGIC  'k'
#define DEMO_IOCTL_SET  _IOW(DEMO_IOC_MAGIC, 1, int)
#define DEMO_IOCTL_GET  _IOR(DEMO_IOC_MAGIC, 2, int)

#define DEV_PATH "/dev/demo"

static void usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s set <int>\n", prog);
    printf("  %s get\n", prog);
    printf("  %s write <string>\n", prog);
    printf("  %s read\n", prog);
}

int main(int argc, char *argv[])
{
    int fd;
    int val;
    char buf[256];
    ssize_t n;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open /dev/demo");
        return 1;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            close(fd);
            return 1;
        }

        val = atoi(argv[2]);
        if (ioctl(fd, DEMO_IOCTL_SET, &val) < 0) {
            perror("ioctl SET");
            close(fd);
            return 1;
        }

        printf("SET ok: %d\n", val);
    } else if (strcmp(argv[1], "get") == 0) {
        if (ioctl(fd, DEMO_IOCTL_GET, &val) < 0) {
            perror("ioctl GET");
            close(fd);
            return 1;
        }

        printf("GET value: %d\n", val);
    } else if (strcmp(argv[1], "write") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            close(fd);
            return 1;
        }

        n = write(fd, argv[2], strlen(argv[2]));
        if (n < 0) {
            perror("write");
            close(fd);
            return 1;
        }

        printf("WRITE ok: %zd bytes\n", n);
    } else if (strcmp(argv[1], "read") == 0) {
        memset(buf, 0, sizeof(buf));
        n = read(fd, buf, sizeof(buf) - 1);
        if (n < 0) {
            perror("read");
            close(fd);
            return 1;
        }

        buf[n] = '\0';
        printf("READ ok: %s\n", buf);
    } else {
        usage(argv[0]);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
