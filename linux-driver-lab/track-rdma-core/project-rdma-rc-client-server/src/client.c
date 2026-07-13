#define _POSIX_C_SOURCE 200112L
#include "rdma_cs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_control_plane_only(const struct rdma_cs_options *options)
{
    struct rdma_cs_metadata local;
    struct rdma_cs_metadata remote;
    int fd = -1;
    int rc = EXIT_FAILURE;

    /*
     * control-plane-only 用假 metadata 测试 client 主动连接和协议解析。
     * 它应该在没有 RDMA device 的机器上也能跑，用来隔离 socket 层问题。
     */
    rdma_cs_options_print(RDMA_CS_ROLE_CLIENT, "control-plane-only", options);
    rdma_cs_log_binding(RDMA_CS_ROLE_CLIENT);
    rdma_cs_metadata_dummy(&local, RDMA_CS_ROLE_CLIENT);
    puts("phase=tcp_connect role=client status=start");
    fd = rdma_cs_tcp_connect(options->server_addr, options->tcp_port);
    if (fd < 0 ||
        rdma_cs_exchange_metadata(fd, &local, &remote) != 0)
        goto out;
    puts("phase=metadata_exchange role=client status=done");

    rdma_cs_metadata_print("client_local_metadata", &local);
    rdma_cs_metadata_print("client_remote_metadata", &remote);
    puts("client_control_plane=pass");
    puts("TCP_CONTROL_PLANE_PASS");
    rc = EXIT_SUCCESS;

out:
    rdma_cs_close_fd(fd);
    return rc;
}

static int run_dry_run(const struct rdma_cs_options *options)
{
    struct rdma_cs_context context;
    struct rdma_cs_metadata local;
    int rc = EXIT_FAILURE;

    /*
     * dry-run 使用 client 自己的 PSN 创建真实 QP/MR/CQ，但不连接 server。
     * 这一步验证“本进程 RDMA 资源生命周期”是否独立成立。
     */
    rdma_cs_options_print(RDMA_CS_ROLE_CLIENT, "dry-run", options);
    rdma_cs_log_binding(RDMA_CS_ROLE_CLIENT);
    puts("phase=resources_create role=client status=start");
    if (rdma_cs_resources_create(&context, options, 0x222222) != 0)
        goto out;
    puts("phase=resources_create role=client status=done");

    rdma_cs_metadata_from_context(&local, &context, RDMA_CS_ROLE_CLIENT);
    rdma_cs_metadata_print("client_local_metadata", &local);
    puts("rdma_resources=created");
    rc = EXIT_SUCCESS;

out:
    rdma_cs_resources_destroy(&context);
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}

static int run_full_client(const struct rdma_cs_options *options)
{
    const char *send_payload = "hello-from-client-send";
    const char *write_payload = "written-by-client-rdma-write";
    const char *read_payload = "read-from-server-rdma-read";
    struct rdma_cs_context context;
    struct rdma_cs_metadata local;
    struct rdma_cs_metadata remote;
    char line[RDMA_CS_LINE_SIZE];
    int fd = -1;
    int rc = EXIT_FAILURE;

    /*
     * full client 的阶段顺序：
     * 1. 创建本地 RDMA 资源；
     * 2. TCP 连接 server 并交换 metadata；
     * 3. 使用 server QPN/PSN/GID 推 QP 到 RTS；
     * 4. 等 server RECV_READY 后发送 SEND；
     * 5. 等 server WRITE_READY 后执行 RDMA WRITE；
     * 6. 等 server READ_READY 后执行 RDMA READ；
     * 7. 使用错误 rkey 触发 remote access error。
     */
    rdma_cs_options_print(RDMA_CS_ROLE_CLIENT, "full", options);
    rdma_cs_log_binding(RDMA_CS_ROLE_CLIENT);
    puts("phase=resources_create role=client status=start");
    if (rdma_cs_resources_create(&context, options, 0x222222) != 0)
        goto out;
    puts("phase=resources_create role=client status=done");
    rdma_cs_metadata_from_context(&local, &context, RDMA_CS_ROLE_CLIENT);

    puts("phase=tcp_connect role=client status=start");
    fd = rdma_cs_tcp_connect(options->server_addr, options->tcp_port);
    if (fd < 0 ||
        rdma_cs_exchange_metadata(fd, &local, &remote) != 0)
        goto out;
    puts("phase=metadata_exchange role=client status=done");

    rdma_cs_metadata_print("client_local_metadata", &local);
    rdma_cs_metadata_print("client_remote_metadata", &remote);
    puts("TCP_CONTROL_PLANE_PASS");

    puts("phase=qp_to_rts role=client status=start");
    if (rdma_cs_qp_to_rts(&context, &remote) != 0)
        goto out;
    puts("client_qp_state=RTS");
    puts("phase=qp_to_rts role=client status=done");
    puts("RC_QP_RTS_PASS");

    if (options->disconnect_after_rts) {
        puts("phase=disconnect_after_rts role=client status=wait_signal");
        if (rdma_cs_recv_line(fd, line, sizeof(line)) != 0 ||
            strcmp(line, "DISCONNECT_AFTER_RTS\n") != 0)
            goto out;
        puts("phase=disconnect_after_rts role=client status=done");
        puts("DISCONNECT_AFTER_RTS_PASS");
        rc = EXIT_SUCCESS;
        goto out;
    }

    puts("phase=send_recv role=client status=wait_ready");
    if (rdma_cs_recv_line(fd, line, sizeof(line)) != 0 ||
        (strcmp(line, "RECV_READY\n") != 0 &&
         strcmp(line, "SKIP_RECV_READY\n") != 0))
        goto out;

    /*
     * SEND/RECV 阶段：正常模式等 server post RECV；skip-recv 模式故意
     * 不提供接收 WR，用它观察 RC RNR retry exceeded。
     */
    puts("phase=send_recv role=client status=start");
    if (rdma_cs_post_send(&context, send_payload, 1001) != 0)
        goto out;
    if (options->skip_recv) {
        if (rdma_cs_poll_expect_error(&context, "client_skip_recv_cqe") != 0 ||
            rdma_cs_send_line(fd, "SKIP_RECV_DONE\n") != 0)
            goto out;
        puts("phase=send_recv role=client mode=skip_recv status=done");
        puts("skip_recv_detected=pass");
        puts("SKIP_RECV_BOUNDARY_PASS");
        rc = EXIT_SUCCESS;
        goto out;
    }
    if (rdma_cs_poll_success(&context, "client_send_cqe") != 0)
        goto out;
    puts("phase=send_recv role=client status=done");
    puts("RC_SEND_RECV_PASS");

    /*
     * RDMA WRITE 阶段：client 本地 CQE success 表示设备完成了写请求；
     * server 侧不会产生 CQE，所以通过 TCP 通知 server 去检查 MR 内容。
     */
    puts("phase=rdma_write role=client status=start");
    if (rdma_cs_recv_line(fd, line, sizeof(line)) != 0 ||
        strcmp(line, "WRITE_READY\n") != 0 ||
        rdma_cs_post_write(&context, &remote, write_payload, 3001, 0, 0) != 0 ||
        rdma_cs_poll_success(&context, "client_write_cqe") != 0 ||
        rdma_cs_send_line(fd, "WRITE_DONE\n") != 0)
        goto out;
    puts("phase=rdma_write role=client status=done");
    puts("RDMA_WRITE_PASS");

    /*
     * RDMA READ 阶段：client 从 server 暴露的 addr/rkey 读数据，
     * 完成事件只出现在 client CQ。
     */
    puts("phase=rdma_read role=client status=start");
    if (rdma_cs_recv_line(fd, line, sizeof(line)) != 0 ||
        strcmp(line, "READ_READY\n") != 0 ||
        rdma_cs_post_read(&context, &remote, strlen(read_payload) + 1,
                          4001) != 0 ||
        rdma_cs_poll_success(&context, "client_read_cqe") != 0 ||
        strcmp(context.buf, read_payload) != 0 ||
        rdma_cs_send_line(fd, "READ_DONE\n") != 0)
        goto out;
    printf("client_read_payload=%s\n", context.buf);
    puts("phase=rdma_read role=client status=done");
    puts("RDMA_READ_PASS");

    /*
     * 错误边界：rkey 被故意改坏后，正确现象是 CQE 非 success。
     * 当前 RXE 返回 remote access error，说明 rkey 确实是远端访问授权边界。
     */
    if (options->wrong_addr) {
        puts("phase=fault_boundary role=client case=wrong_addr status=start");
        if (rdma_cs_post_write(&context, &remote, "bad-addr-write",
                               5000, 0, 1) != 0 ||
            rdma_cs_poll_expect_error(&context, "client_wrong_addr_cqe") != 0 ||
            rdma_cs_send_line(fd, "WRONG_ADDR_DONE\n") != 0)
            goto out;
        puts("phase=fault_boundary role=client case=wrong_addr status=done");
        puts("wrong_addr_detected=pass");
        puts("WRONG_ADDR_BOUNDARY_PASS");
        rc = EXIT_SUCCESS;
        goto out;
    }

    puts("phase=fault_boundary role=client case=wrong_rkey status=start");
    if (rdma_cs_post_write(&context, &remote, "bad-rkey-write", 5001, 1, 0) != 0 ||
        rdma_cs_poll_expect_error(&context, "client_wrong_rkey_cqe") != 0 ||
        rdma_cs_send_line(fd, "WRONG_RKEY_DONE\n") != 0)
        goto out;
    puts("phase=fault_boundary role=client case=wrong_rkey status=done");
    puts("wrong_rkey_detected=pass");
    puts("WRONG_RKEY_BOUNDARY_PASS");

    rc = EXIT_SUCCESS;

out:
    rdma_cs_close_fd(fd);
    rdma_cs_resources_destroy(&context);
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}

int main(int argc, char **argv)
{
    struct rdma_cs_options options;
    int parsed = rdma_cs_parse_common(argc, argv, &options,
                                      RDMA_CS_ROLE_CLIENT);

    if (parsed > 0)
        return EXIT_SUCCESS;
    if (parsed < 0)
        return EXIT_FAILURE;

    if (options.control_plane_only)
        return run_control_plane_only(&options);
    if (options.dry_run)
        return run_dry_run(&options);

    return run_full_client(&options);
}
