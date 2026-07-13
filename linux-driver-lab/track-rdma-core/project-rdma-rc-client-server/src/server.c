#define _POSIX_C_SOURCE 200112L
#include "rdma_cs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_control_plane_only(const struct rdma_cs_options *options)
{
    struct rdma_cs_metadata local;
    struct rdma_cs_metadata remote;
    int listen_fd = -1;
    int conn_fd = -1;
    int rc = EXIT_FAILURE;

    /*
     * control-plane-only 只测试 TCP 和 metadata 协议。
     * 这一步不依赖 rxe0，因此可以把 socket/解析问题和 RDMA 环境问题分开。
     */
    rdma_cs_options_print(RDMA_CS_ROLE_SERVER, "control-plane-only", options);
    rdma_cs_log_binding(RDMA_CS_ROLE_SERVER);
    rdma_cs_metadata_dummy(&local, RDMA_CS_ROLE_SERVER);
    listen_fd = rdma_cs_tcp_listen(options->listen_addr, options->tcp_port);
    if (listen_fd < 0)
        goto out;

    printf("server_listen=%s:%s\n", options->listen_addr, options->tcp_port);
    puts("phase=tcp_accept role=server status=waiting");
    conn_fd = rdma_cs_tcp_accept(listen_fd);
    if (conn_fd < 0 ||
        rdma_cs_exchange_metadata(conn_fd, &local, &remote) != 0)
        goto out;
    puts("phase=metadata_exchange role=server status=done");

    rdma_cs_metadata_print("server_local_metadata", &local);
    rdma_cs_metadata_print("server_remote_metadata", &remote);
    puts("server_control_plane=pass");
    puts("TCP_CONTROL_PLANE_PASS");
    rc = EXIT_SUCCESS;

out:
    rdma_cs_close_fd(conn_fd);
    rdma_cs_close_fd(listen_fd);
    return rc;
}

static int run_dry_run(const struct rdma_cs_options *options)
{
    struct rdma_cs_context context;
    struct rdma_cs_metadata local;
    int rc = EXIT_FAILURE;

    /*
     * dry-run 创建真实 RDMA 资源，但不和对端交换 metadata。
     * 它用于回答：当前机器能不能打开 rxe0 并创建 PD/MR/CQ/QP？
     */
    rdma_cs_options_print(RDMA_CS_ROLE_SERVER, "dry-run", options);
    rdma_cs_log_binding(RDMA_CS_ROLE_SERVER);
    puts("phase=resources_create role=server status=start");
    if (rdma_cs_resources_create(&context, options, 0x111111) != 0)
        goto out;
    puts("phase=resources_create role=server status=done");

    rdma_cs_metadata_from_context(&local, &context, RDMA_CS_ROLE_SERVER);
    rdma_cs_metadata_print("server_local_metadata", &local);
    puts("rdma_resources=created");
    rc = EXIT_SUCCESS;

out:
    rdma_cs_resources_destroy(&context);
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}

static int run_full_server(const struct rdma_cs_options *options)
{
    const char *read_payload = "read-from-server-rdma-read";
    const char *write_payload = "written-by-client-rdma-write";
    struct rdma_cs_context context;
    struct rdma_cs_metadata local;
    struct rdma_cs_metadata remote;
    char line[RDMA_CS_LINE_SIZE];
    int listen_fd = -1;
    int conn_fd = -1;
    int rc = EXIT_FAILURE;

    /*
     * full server 的阶段顺序：
     * 1. 创建本地 RDMA 资源；
     * 2. TCP 等 client 连接并交换 metadata；
     * 3. 根据 client metadata 把 QP 推到 RTS；
     * 4. 先 post RECV，再通知 client 可以 SEND；
     * 5. 验证 RDMA WRITE 改写了 server MR；
     * 6. 准备 RDMA READ 的源数据；
     * 7. 接收 wrong-rkey case 的完成通知。
     */
    rdma_cs_options_print(RDMA_CS_ROLE_SERVER, "full", options);
    rdma_cs_log_binding(RDMA_CS_ROLE_SERVER);
    puts("phase=resources_create role=server status=start");
    if (rdma_cs_resources_create(&context, options, 0x111111) != 0)
        goto out;
    puts("phase=resources_create role=server status=done");
    rdma_cs_metadata_from_context(&local, &context, RDMA_CS_ROLE_SERVER);

    puts("phase=tcp_listen role=server status=start");
    listen_fd = rdma_cs_tcp_listen(options->listen_addr, options->tcp_port);
    if (listen_fd < 0)
        goto out;
    printf("server_listen=%s:%s\n", options->listen_addr, options->tcp_port);

    puts("phase=tcp_accept role=server status=waiting");
    conn_fd = rdma_cs_tcp_accept(listen_fd);
    if (conn_fd < 0 ||
        rdma_cs_exchange_metadata(conn_fd, &local, &remote) != 0)
        goto out;
    puts("phase=metadata_exchange role=server status=done");

    rdma_cs_metadata_print("server_local_metadata", &local);
    rdma_cs_metadata_print("server_remote_metadata", &remote);
    puts("TCP_CONTROL_PLANE_PASS");

    puts("phase=qp_to_rts role=server status=start");
    if (rdma_cs_qp_to_rts(&context, &remote) != 0)
        goto out;
    puts("server_qp_state=RTS");
    puts("phase=qp_to_rts role=server status=done");
    puts("RC_QP_RTS_PASS");

    if (options->disconnect_after_rts) {
        puts("phase=disconnect_after_rts role=server status=start");
        if (rdma_cs_send_line(conn_fd, "DISCONNECT_AFTER_RTS\n") != 0)
            goto out;
        puts("phase=disconnect_after_rts role=server status=done");
        puts("DISCONNECT_AFTER_RTS_PASS");
        rc = EXIT_SUCCESS;
        goto out;
    }

    /*
     * SEND/RECV 阶段必须由 server 先 post_recv，再通过 TCP 告诉 client。
     * TCP 在这里不是数据面，只是一个同步栅栏，避免 RNR。
     */
    memset(context.buf, 0, RDMA_CS_BUFFER_SIZE);
    if (options->skip_recv) {
        puts("phase=send_recv role=server mode=skip_recv status=start");
        if (rdma_cs_send_line(conn_fd, "SKIP_RECV_READY\n") != 0 ||
            rdma_cs_recv_line(conn_fd, line, sizeof(line)) != 0 ||
            strcmp(line, "SKIP_RECV_DONE\n") != 0)
            goto out;
        puts("phase=send_recv role=server mode=skip_recv status=done");
        puts("SKIP_RECV_BOUNDARY_PASS");
        rc = EXIT_SUCCESS;
        goto out;
    } else {
        puts("phase=send_recv role=server status=start");
        if (rdma_cs_post_recv(&context, 2002) != 0 ||
            rdma_cs_send_line(conn_fd, "RECV_READY\n") != 0 ||
            rdma_cs_poll_success(&context, "server_recv_cqe") != 0)
            goto out;
    }
    printf("server_recv_payload=%s\n", context.buf);
    if (strcmp(context.buf, "hello-from-client-send") != 0)
        goto out;
    puts("phase=send_recv role=server status=done");
    puts("RC_SEND_RECV_PASS");

    /*
     * WRITE 阶段 server 不 post RECV。client 会使用 server 的 addr/rkey
     * 直接写 server MR；server 只通过 TCP 等待 client 通知 WRITE_DONE。
     */
    puts("phase=rdma_write role=server status=start");
    if (rdma_cs_send_line(conn_fd, "WRITE_READY\n") != 0 ||
        rdma_cs_recv_line(conn_fd, line, sizeof(line)) != 0 ||
        strcmp(line, "WRITE_DONE\n") != 0 ||
        strcmp(context.buf, write_payload) != 0)
        goto out;
    printf("server_write_payload=%s\n", context.buf);
    puts("phase=rdma_write role=server status=done");
    puts("RDMA_WRITE_PASS");

    /*
     * READ 阶段 server 只准备源数据。client 通过 RDMA READ 把数据拉到
     * 自己的本地 MR，server 侧没有 READ CQE。
     */
    puts("phase=rdma_read role=server status=start");
    snprintf(context.buf, RDMA_CS_BUFFER_SIZE, "%s", read_payload);
    if (rdma_cs_send_line(conn_fd, "READ_READY\n") != 0 ||
        rdma_cs_recv_line(conn_fd, line, sizeof(line)) != 0 ||
        strcmp(line, "READ_DONE\n") != 0)
        goto out;
    puts("phase=rdma_read role=server status=done");
    puts("RDMA_READ_PASS");

    if (options->wrong_addr &&
        rdma_cs_recv_line(conn_fd, line, sizeof(line)) == 0 &&
        strcmp(line, "WRONG_ADDR_DONE\n") == 0) {
        puts("phase=fault_boundary role=server case=wrong_addr status=done");
        puts("WRONG_ADDR_BOUNDARY_PASS");
        rc = EXIT_SUCCESS;
        goto out;
    }

    if (!options->wrong_addr &&
        rdma_cs_recv_line(conn_fd, line, sizeof(line)) == 0 &&
        strcmp(line, "WRONG_RKEY_DONE\n") == 0) {
        puts("phase=fault_boundary role=server case=wrong_rkey status=done");
        puts("WRONG_RKEY_BOUNDARY_PASS");
        rc = EXIT_SUCCESS;
        goto out;
    }

out:
    rdma_cs_close_fd(conn_fd);
    rdma_cs_close_fd(listen_fd);
    rdma_cs_resources_destroy(&context);
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}

int main(int argc, char **argv)
{
    struct rdma_cs_options options;
    int parsed = rdma_cs_parse_common(argc, argv, &options,
                                      RDMA_CS_ROLE_SERVER);

    if (parsed > 0)
        return EXIT_SUCCESS;
    if (parsed < 0)
        return EXIT_FAILURE;

    if (options.control_plane_only)
        return run_control_plane_only(&options);
    if (options.dry_run)
        return run_dry_run(&options);

    return run_full_server(&options);
}
