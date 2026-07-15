#include "gateway_rdma_worker.h"

#include <errno.h>
#include <sched.h>
#include <string.h>

void gateway_rdma_worker_init(struct gateway_rdma_worker *worker,
                              struct gateway_ingress_context *ingress,
                              struct gateway_rdma_backend *backend)
{
    memset(worker, 0, sizeof(*worker));
    worker->ingress = ingress;
    worker->backend = backend;
    atomic_init(&worker->stop, 0);
}

void gateway_rdma_worker_stop(struct gateway_rdma_worker *worker)
{
    /* release 与 worker acquire 配对，发布 producer 已结束的事实。 */
    atomic_store_explicit(&worker->stop, 1, memory_order_release);
}

void *gateway_rdma_worker_run(void *argument)
{
    struct gateway_rdma_worker *worker = argument;

    for (;;) {
        struct gateway_request request;
        int ret = gateway_ring_dequeue(&worker->ingress->request_ring,
                                       &request);

        if (ret == -ENOENT) {
            /* stop 后仍先确认 ring 为空，避免退出时丢掉已发布 descriptor。 */
            if (atomic_load_explicit(&worker->stop, memory_order_acquire))
                break;
            sched_yield();
            continue;
        }
        if (ret != 0) {
            worker->stats.errors++;
            break;
        }

        worker->stats.dequeued_requests++;
        if (gateway_slot_mark_inflight(&worker->ingress->slot_pool,
                                       request.slot_id,
                                       request.generation) != 0 ||
            gateway_rdma_write_request(
                worker->backend, &request,
                worker->ingress->staging[request.slot_id].payload) != 0 ||
            gateway_slot_complete(&worker->ingress->slot_pool,
                                  request.slot_id,
                                  request.generation) != 0) {
            worker->stats.errors++;
            break;
        }
        worker->stats.completed_requests++;
        worker->stats.payload_bytes += request.payload_len;
        worker->stats.write_bytes += GATEWAY_WIRE_HEADER_SIZE +
                                     request.payload_len;
    }
    return NULL;
}
