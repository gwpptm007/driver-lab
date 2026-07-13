#define _POSIX_C_SOURCE 200112L
#include "rdma_cs.h"
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int set_reuseaddr(int fd)
{
    int yes = 1;

    /*
     * 测试脚本会反复启动 server。SO_REUSEADDR 避免上一次连接进入
     * TIME_WAIT 后，下一次 bind 127.0.0.1:18515/18516 失败。
     */
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
}

int rdma_cs_tcp_listen(const char *host, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *ai;
    int fd = -1;

    /*
     * getaddrinfo 让代码同时兼容 IPv4/IPv6。当前测试使用 127.0.0.1，
     * 后续双机 RoCEv2 时可以直接传入测试网卡 IP。
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(host, port, &hints, &result) != 0)
        return -1;

    for (ai = result; ai != NULL; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;
        (void)set_reuseaddr(fd);
        if (bind(fd, ai->ai_addr, ai->ai_addrlen) == 0 &&
            listen(fd, 1) == 0)
            break;
        close(fd);
        fd = -1;
    }

    freeaddrinfo(result);
    return fd;
}

int rdma_cs_tcp_accept(int listen_fd)
{
    return accept(listen_fd, NULL, NULL);
}

int rdma_cs_tcp_connect(const char *host, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *ai;
    int fd = -1;

    /*
     * client 只负责主动连接控制面。RDMA 数据面不会复用这个 socket，
     * 这个 fd 只用于交换 metadata 和几个阶段同步信号。
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &result) != 0)
        return -1;

    for (ai = result; ai != NULL; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }

    freeaddrinfo(result);
    return fd;
}

static int send_all(int fd, const char *buf, size_t len)
{
    size_t sent = 0;

    /*
     * send 可能只写出部分字节。控制面消息虽然很短，但这里仍然按
     * socket 的正确语义循环发送，避免以后扩展字段后踩坑。
     */
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);

        if (n <= 0)
            return -1;
        sent += (size_t)n;
    }
    return 0;
}

int rdma_cs_recv_line(int fd, char *buf, size_t len)
{
    size_t used = 0;

    /*
     * 控制面协议以换行符作为一条消息边界。
     * 这让 server/client 可以交替发送 "RECV_READY"、"WRITE_DONE"
     * 这类同步消息，而不需要引入复杂 framing。
     */
    while (used + 1 < len) {
        char ch;
        ssize_t n = recv(fd, &ch, 1, 0);

        if (n <= 0)
            return -1;
        buf[used++] = ch;
        if (ch == '\n') {
            buf[used] = '\0';
            return 0;
        }
    }
    return -1;
}

int rdma_cs_send_line(int fd, const char *line)
{
    return send_all(fd, line, strlen(line));
}

int rdma_cs_exchange_metadata(int fd,
                              const struct rdma_cs_metadata *local,
                              struct rdma_cs_metadata *remote)
{
    char send_line[RDMA_CS_LINE_SIZE];
    char recv_buf[RDMA_CS_LINE_SIZE];

    /*
     * 双方都先 send 再 recv；一行 metadata 很小，不会填满 socket buffer。
     * 这一步完成后，双方才知道对端 QPN/PSN/GID/address/rkey，
     * 也才具备把 QP 推到 RTR/RTS 的信息。
     */
    if (rdma_cs_metadata_format(local, send_line, sizeof(send_line)) != 0 ||
        send_all(fd, send_line, strlen(send_line)) != 0 ||
        rdma_cs_recv_line(fd, recv_buf, sizeof(recv_buf)) != 0 ||
        rdma_cs_metadata_parse(recv_buf, remote) != 0)
        return -1;

    return 0;
}

void rdma_cs_close_fd(int fd)
{
    if (fd >= 0)
        close(fd);
}
