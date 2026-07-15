#ifndef GATEWAY_RDMA_BACKEND_H
#define GATEWAY_RDMA_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#include "gateway_contract.h"
#include "rdma_cs.h"

struct gateway_rdma_backend {
    struct rdma_cs_context verbs;
    struct rdma_cs_metadata local;
    struct rdma_cs_metadata remote;
    enum rdma_cs_role role;
};

/* create/connect 管理 verbs 对象和 QP 建链；WRITE payload 不经过 TCP。 */
int gateway_rdma_backend_create(struct gateway_rdma_backend *backend,
                                const struct rdma_cs_options *options,
                                enum rdma_cs_role role, uint32_t psn);
int gateway_rdma_backend_connect(struct gateway_rdma_backend *backend,
                                 int control_fd);
int gateway_rdma_write_request(struct gateway_rdma_backend *backend,
                               const struct gateway_request *request,
                               const uint8_t *payload);
int gateway_rdma_validate_remote(struct gateway_rdma_backend *backend,
                                 uint64_t expected_request_id,
                                 const uint8_t *expected_payload,
                                 size_t expected_payload_len);
void gateway_rdma_backend_destroy(struct gateway_rdma_backend *backend);

#endif
