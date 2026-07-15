#include "gateway_rdma_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GATEWAY_PHASE3_REQUEST_ID 3001ULL
#define GATEWAY_PHASE3_PAYLOAD_LEN 32U

static int run_client(const struct rdma_cs_options *options)
{
    struct gateway_rdma_backend backend;
    struct gateway_slot_pool slots;
    struct gateway_staging_slot staging;
    struct gateway_request request;
    char line[RDMA_CS_LINE_SIZE];
    uint32_t generation;
    int fd = -1;
    int rc = EXIT_FAILURE;

    memset(&backend, 0, sizeof(backend));
    gateway_slot_pool_init(&slots);
    memset(&staging, '.', sizeof(staging));
    memcpy(staging.payload, "GATEWAY_RDMA_PHASE3",
           strlen("GATEWAY_RDMA_PHASE3"));
    if (gateway_slot_prepare(&slots, 3, GATEWAY_PHASE3_PAYLOAD_LEN,
                             &generation) != 0)
        goto out;

    memset(&request, 0, sizeof(request));
    request.request_id = GATEWAY_PHASE3_REQUEST_ID;
    request.flow_hash = 0x0303030303030303ULL;
    request.slot_id = 3;
    request.generation = generation;
    request.payload_len = GATEWAY_PHASE3_PAYLOAD_LEN;
    request.opcode = GATEWAY_OP_RDMA_WRITE;

    if (gateway_rdma_backend_create(&backend, options, RDMA_CS_ROLE_CLIENT,
                                    0x23456U) != 0)
        goto out;
    fd = rdma_cs_tcp_connect(options->server_addr, options->tcp_port);
    if (fd < 0 || gateway_rdma_backend_connect(&backend, fd) != 0)
        goto out;
    puts("GATEWAY_RDMA_QP_RTS_PASS role=client");

    /* 只有 signaled WRITE CQE 成功后，当前 generation 的 slot 才能回到 FREE。 */
    if (gateway_slot_mark_inflight(&slots, request.slot_id,
                                   request.generation) != 0 ||
        gateway_rdma_write_request(&backend, &request, staging.payload) != 0 ||
        gateway_slot_complete(&slots, request.slot_id,
                              request.generation) != 0)
        goto out;
    puts("GATEWAY_RDMA_WRITE_CQE_PASS request_id=3001 bytes=72");
    puts("GATEWAY_RDMA_SLOT_COMPLETE_PASS slot=3 generation=1");

    if (rdma_cs_send_line(fd, "WRITE_DONE request=3001 bytes=72\n") != 0 ||
        rdma_cs_recv_line(fd, line, sizeof(line)) != 0 ||
        strcmp(line, "REMOTE_VALIDATED\n") != 0)
        goto out;
    puts("DPDK_RDMA_GATEWAY_PHASE3_RDMA_PASS");
    rc = EXIT_SUCCESS;

out:
    rdma_cs_close_fd(fd);
    gateway_rdma_backend_destroy(&backend);
    printf("cleanup=complete role=client result=%s\n",
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
    return run_client(&options);
}
