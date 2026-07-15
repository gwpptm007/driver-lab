#include "gateway_rdma_backend.h"

#include <errno.h>
#include <string.h>

int gateway_rdma_backend_create(struct gateway_rdma_backend *backend,
                                const struct rdma_cs_options *options,
                                enum rdma_cs_role role, uint32_t psn)
{
    memset(backend, 0, sizeof(*backend));
    backend->role = role;
    if (rdma_cs_resources_create(&backend->verbs, options, psn) != 0)
        return -1;
    return rdma_cs_metadata_from_context(&backend->local, &backend->verbs,
                                         role);
}

int gateway_rdma_backend_connect(struct gateway_rdma_backend *backend,
                                 int control_fd)
{
    /* TCP 只交换 QPN/PSN/GID/MR 元数据，业务 payload 不经过控制面。 */
    if (rdma_cs_exchange_metadata(control_fd, &backend->local,
                                  &backend->remote) != 0)
        return -1;
    return rdma_cs_qp_to_rts(&backend->verbs, &backend->remote);
}

int gateway_rdma_write_request(struct gateway_rdma_backend *backend,
                               const struct gateway_request *request,
                               const uint8_t *payload)
{
    uint8_t record[GATEWAY_WIRE_HEADER_SIZE + GATEWAY_MAX_PAYLOAD];
    size_t record_len;

    if (payload == NULL || gateway_request_validate(request) != 0)
        return -EINVAL;
    record_len = GATEWAY_WIRE_HEADER_SIZE + request->payload_len;
    if (record_len > RDMA_CS_BUFFER_SIZE)
        return -EMSGSIZE;
    if (gateway_wire_encode(request, record, sizeof(record)) != 0)
        return -EINVAL;
    memcpy(record + GATEWAY_WIRE_HEADER_SIZE, payload, request->payload_len);

    /* rdma_cs 把完整 record 复制到已注册 send MR，再 post signaled WRITE。 */
    if (rdma_cs_post_write_at(&backend->verbs, record, record_len,
                              backend->remote.addr, backend->remote.rkey,
                              request->request_id) != 0)
        return -1;
    return rdma_cs_poll_success(&backend->verbs, "gateway_rdma_write_cqe");
}

int gateway_rdma_validate_remote(struct gateway_rdma_backend *backend,
                                 uint64_t expected_request_id,
                                 const uint8_t *expected_payload,
                                 size_t expected_payload_len)
{
    struct gateway_request decoded;
    const uint8_t *record = (const uint8_t *)backend->verbs.buf;

    if (expected_payload == NULL ||
        gateway_wire_decode(record, RDMA_CS_BUFFER_SIZE, &decoded) != 0)
        return -EINVAL;
    if (decoded.request_id != expected_request_id ||
        decoded.payload_len != expected_payload_len ||
        memcmp(record + GATEWAY_WIRE_HEADER_SIZE, expected_payload,
               expected_payload_len) != 0)
        return -EPROTO;
    return 0;
}

void gateway_rdma_backend_destroy(struct gateway_rdma_backend *backend)
{
    rdma_cs_resources_destroy(&backend->verbs);
}
