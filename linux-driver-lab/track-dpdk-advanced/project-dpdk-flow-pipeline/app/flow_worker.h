#ifndef FLOW_WORKER_H
#define FLOW_WORKER_H

#include <rte_mempool.h>

/* 使用调用方 mempool 构造报文，验证双 worker 的 shared/sharded 模型。 */
int flow_worker_model_selftest(struct rte_mempool *pool);

#endif
