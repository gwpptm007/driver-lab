#include "gateway_rdma_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GATEWAY_PHASE3_REQUEST_ID 3001ULL
#define GATEWAY_PHASE3_PAYLOAD_LEN 32U

static int run_server(const struct rdma_cs_options *options)
{
    struct gateway_rdma_backend backend;
    uint8_t expected[GATEWAY_PHASE3_PAYLOAD_LEN];
    char line[RDMA_CS_LINE_SIZE];
    int listen_fd = -1;
    int conn_fd = -1;
    int rc = EXIT_FAILURE;

    memset(&backend, 0, sizeof(backend));
    memset(expected, '.', sizeof(expected));
    memcpy(expected, "GATEWAY_RDMA_PHASE3", strlen("GATEWAY_RDMA_PHASE3"));
    if (gateway_rdma_backend_create(&backend, options, RDMA_CS_ROLE_SERVER,
                                    0x12345U) != 0)
        goto out;
    listen_fd = rdma_cs_tcp_listen(options->listen_addr, options->tcp_port);
    if (listen_fd < 0)
        goto out;
    conn_fd = rdma_cs_tcp_accept(listen_fd);
    if (conn_fd < 0 || gateway_rdma_backend_connect(&backend, conn_fd) != 0)
        goto out;
    puts("GATEWAY_RDMA_QP_RTS_PASS role=server");
    puts("GATEWAY_RDMA_SERVER_READY");

    /* one-sided WRITE 在远端不产生 CQE，TCP token 只用于测试同步后验收 MR。 */
    if (rdma_cs_recv_line(conn_fd, line, sizeof(line)) != 0 ||
        strcmp(line, "WRITE_DONE request=3001 bytes=72\n") != 0 ||
        gateway_rdma_validate_remote(&backend, GATEWAY_PHASE3_REQUEST_ID,
                                     expected, sizeof(expected)) != 0)
        goto out;
    puts("GATEWAY_REMOTE_RECORD_PASS request_id=3001 payload_bytes=32");
    if (rdma_cs_send_line(conn_fd, "REMOTE_VALIDATED\n") != 0)
        goto out;
    puts("DPDK_RDMA_GATEWAY_PHASE3_SERVER_PASS");
    rc = EXIT_SUCCESS;

out:
    rdma_cs_close_fd(conn_fd);
    rdma_cs_close_fd(listen_fd);
    gateway_rdma_backend_destroy(&backend);
    printf("cleanup=complete role=server result=%s\n",
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
    return run_server(&options);
}
