#ifndef GATEWAY_RDMA_WORKER_H
#define GATEWAY_RDMA_WORKER_H

#include <stdatomic.h>
#include <stdint.h>

#include "gateway_ingress.h"
#include "gateway_rdma_backend.h"

struct gateway_rdma_worker_stats {
    uint64_t dequeued_requests;
    uint64_t completed_requests;
    uint64_t payload_bytes;
    uint64_t write_bytes;
    uint64_t errors;
};

struct gateway_rdma_worker {
    struct gateway_ingress_context *ingress;
    struct gateway_rdma_backend *backend;
    struct gateway_rdma_worker_stats stats;
    _Atomic int stop;
};

/* stop 只停止新生产，worker 必须 drain ring 后再退出。 */
void gateway_rdma_worker_init(struct gateway_rdma_worker *worker,
                              struct gateway_ingress_context *ingress,
                              struct gateway_rdma_backend *backend);
void gateway_rdma_worker_stop(struct gateway_rdma_worker *worker);
void *gateway_rdma_worker_run(void *argument);

#endif
