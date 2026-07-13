#ifndef FLOW_PIPELINE_H
#define FLOW_PIPELINE_H

#include <stdint.h>

#include "flow_table.h"

#define FLOW_LATENCY_MAX_SAMPLES 65536U

/* classify 只返回动作决策，不转移或释放输入 mbuf 的所有权。 */
struct flow_decision {
    enum flow_action_type action;
    uint16_t out_port;
    uint32_t mark_id;
    int table_hit;
};

struct flow_pipeline_stats {
    uint64_t hash_hits;
    uint64_t hash_misses;
    uint64_t rule_drops;
    uint64_t forwards;
    uint64_t marks;
    uint64_t default_drops;
    uint64_t invalid_packets;
    uint64_t latency_cycles[FLOW_LATENCY_MAX_SAMPLES];
    uint32_t latency_count;
};

/* owns_table 区分自有流表与 worker 附着的共享流表，决定 destroy 行为。 */
struct flow_pipeline {
    struct flow_table owned_table;
    struct flow_table *table;
    struct flow_pipeline_stats stats;
    int owns_table;
};

/* create 创建并持有流表；attach 仅借用调用方保证存活的共享流表。 */
int flow_pipeline_create(struct flow_pipeline *pipeline, uint16_t out_port);
void flow_pipeline_attach(struct flow_pipeline *pipeline,
                          struct flow_table *shared_table);
int flow_pipeline_classify(struct flow_pipeline *pipeline,
                           const struct rte_mbuf *mbuf,
                           struct flow_decision *decision);
/* print 在 fast path 结束后离线计算分位数并输出规则统计。 */
void flow_pipeline_print(const struct flow_pipeline *pipeline);
void flow_pipeline_destroy(struct flow_pipeline *pipeline);

#endif
