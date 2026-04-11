#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEMO_IOCTL_MAGIC 'k'
#define DEMO_IOCTL_GET _IOR(DEMO_IOCTL_MAGIC, 1, int)

int main() {
    int fd = open("/dev/demo_day04", O_RDWR);
    if (fd < 0) {
        perror("Open /dev/demo_day04 failed");
        return 1;
    }
    ioctl(fd, DEMO_IOCTL_GET, NULL);
    printf("IOCTL Sent to Driver\n");
    close(fd);
    return 0;
}
