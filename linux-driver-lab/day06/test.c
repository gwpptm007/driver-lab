#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>

/*
 * 用户态小工具是 Day04 ~ Day06 的“统一测试入口”
 *
 * 为什么保留一个独立的 test 程序，而不是全靠 shell 直接 echo/cat？
 *
 * 原因：
 * 1. ioctl 无法靠 echo/cat 直接覆盖，必须自己写程序发命令。
 * 2. errno 原样返回给脚本，方便脚本判断：
 *    - 16  -> EBUSY，当前单槽 workqueue 模型下是预期竞争结果
 *    - 124 -> 人为约定的“读超时”退出码
 *    - 其他 -> 脚本记为错误
 *
 * Day06 作用：
 * - 手工交互：敲命令看驱动行为
 * - 脚本调用： all.sh / stress_rw.sh / insmod_rmmod.sh 基础测试
 */

#define DEMO_IOC_MAGIC  'k'
#define DEMO_IOCTL_SET  _IOW(DEMO_IOC_MAGIC, 1, int)
#define DEMO_IOCTL_GET  _IOR(DEMO_IOC_MAGIC, 2, int)

#define DEV_PATH "/dev/demo"

/*
 * 使用 alarm(2) 给 read() 增加“超时保护”。
 *
 * shell 脚本最怕的是某个 read 永久阻塞，把整轮回归卡死。
 *
 * 处理思路：
 * - read_timeout N 先安装 SIGALRM 处理函数
 * - alarm(N) 后再执行 read()
 * - 如果 N 秒内没读到数据，内核会向当前进程发送 SIGALRM
 * - 在信号处理函数里把 g_timed_out 置 1
 * - read() 被 EINTR 打断后，main() 根据这个标志返回 124
 */
static volatile sig_atomic_t g_timed_out;

static void usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s set <int>\n", prog);
    printf("  %s get\n", prog);
    printf("  %s write <string>\n", prog);
    printf("  %s read\n", prog);
    printf("  %s read_timeout <seconds>\n", prog);
}

static void alarm_handler(int signo)
{
    (void)signo;
    g_timed_out = 1;
}

/*
 * 把 errno 尽量转换成 shell 可识别的退出码。
 *
 * 正常 shell 退出码范围是 0~255，所以做一个简单兜底：
 * - <=0 或 >255 的情况统一返回 1
 * - 常见 errno（如 16/22/110 等）则直接透传
 *
 * 这样脚本里就可以直接 case "$RC" in 16|124|...)。
 */
static int errno_to_exit_code(int err)
{
    if (err <= 0)
        return 1;
    if (err > 255)
        return 1;
    return err;
}

int main(int argc, char *argv[])
{
    int fd;
    int val;
    int saved_errno;
    char buf[256];
    ssize_t n;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    /*
     * 所有子命令都先打开 /dev/demo。
     * 保证：
     * - 设备节点是否存在
     * - open 路径是否正常
     * 都会被统一覆盖到
     */
    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open /dev/demo");
        return errno_to_exit_code(errno);
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            close(fd);
            return 1;
        }

        val = atoi(argv[2]);
        if (ioctl(fd, DEMO_IOCTL_SET, &val) < 0) {
            saved_errno = errno;
            perror("ioctl SET");
            close(fd);
            return errno_to_exit_code(saved_errno);
        }

        printf("SET ok: %d\n", val);
    } else if (strcmp(argv[1], "get") == 0) {
        if (ioctl(fd, DEMO_IOCTL_GET, &val) < 0) {
            saved_errno = errno;
            perror("ioctl GET");
            close(fd);
            return errno_to_exit_code(saved_errno);
        }

        printf("GET ok: %d\n", val);
    } else if (strcmp(argv[1], "write") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            close(fd);
            return 1;
        }

        n = write(fd, argv[2], strlen(argv[2]));
        if (n < 0) {
            saved_errno = errno;
            perror("write");
            close(fd);
            return errno_to_exit_code(saved_errno);
        }

        printf("WRITE ok: %zd bytes\n", n);
    } else if (strcmp(argv[1], "read") == 0) {
        /*
         * 普通 read：适合手工实验
         * 如果驱动侧还没有准备好 data_ready，这里会阻塞
         */
        memset(buf, 0, sizeof(buf));
        n = read(fd, buf, sizeof(buf) - 1);
        if (n < 0) {
            saved_errno = errno;
            perror("read");
            close(fd);
            return errno_to_exit_code(saved_errno);
        }

        buf[n] = '\0';
        printf("READ ok: %s\n", buf);
    } else if (strcmp(argv[1], "read_timeout") == 0) {
        int timeout_sec;

        if (argc != 3) {
            usage(argv[0]);
            close(fd);
            return 1;
        }

        timeout_sec = atoi(argv[2]);
        if (timeout_sec <= 0) {
            fprintf(stderr, "invalid timeout: %s\n", argv[2]);
            close(fd);
            return 1;
        }

        /*
         * 给“阻塞读”增加一个时间上限。
         * 这里不用 signal()，而用 sigaction() 且不设置 SA_RESTART。
         * 目的就是让阻塞 read() 在收到 SIGALRM 后更稳定地返回 EINTR。
         * 这样脚本里的 reader 才能按预期超时退出，不会长时间挂在 wait_event_interruptible() 上。
         */
        {
            struct sigaction sa;

            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = alarm_handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            if (sigaction(SIGALRM, &sa, NULL) < 0) {
                saved_errno = errno;
                perror("sigaction SIGALRM");
                close(fd);
                return errno_to_exit_code(saved_errno);
            }
        }

        g_timed_out = 0;
        alarm(timeout_sec);

        memset(buf, 0, sizeof(buf));
        n = read(fd, buf, sizeof(buf) - 1);
        saved_errno = errno;

        /*
         * 无论成功还是失败，先把定时器清掉，避免影响后续逻辑
         */
        alarm(0);

        if (n < 0) {
            /*
             * 如果是被 SIGALRM 打断，并且确认超时标志已经置位，就约定返回 124
             * 124 不是内核 errno，而是脚本层约定的“超时”退出码
             */
            if (saved_errno == EINTR && g_timed_out) {
                fprintf(stderr, "read timeout after %d seconds\n", timeout_sec);
                close(fd);
                return 124;
            }

            perror("read_timeout/read");
            close(fd);
            return errno_to_exit_code(saved_errno);
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
