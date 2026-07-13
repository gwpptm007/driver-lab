#include "flow_pipeline.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_cycles.h>

static int compare_u64(const void *left, const void *right)
{
    const uint64_t a = *(const uint64_t *)left;
    const uint64_t b = *(const uint64_t *)right;

    return (a > b) - (a < b);
}

static void record_latency(struct flow_pipeline_stats *stats, uint64_t start)
{
    /* 样本只覆盖 parse + hash + decision，不包含 RX/TX 和 PMD 时延。 */
    if (stats->latency_count < FLOW_LATENCY_MAX_SAMPLES)
        stats->latency_cycles[stats->latency_count++] = rte_rdtsc() - start;
}

int flow_pipeline_create(struct flow_pipeline *pipeline, uint16_t out_port)
{
    memset(pipeline, 0, sizeof(*pipeline));
    pipeline->table = &pipeline->owned_table;
    pipeline->owns_table = 1;
    return flow_table_create(pipeline->table, out_port);
}

void flow_pipeline_attach(struct flow_pipeline *pipeline,
                          struct flow_table *shared_table)
{
    memset(pipeline, 0, sizeof(*pipeline));
    pipeline->table = shared_table;
}

int flow_pipeline_classify(struct flow_pipeline *pipeline,
                           const struct rte_mbuf *mbuf,
                           struct flow_decision *decision)
{
    struct flow_key key;
    struct flow_rule *rule;
    uint64_t start = rte_rdtsc();
    int ret;

    memset(decision, 0, sizeof(*decision));
    decision->action = FLOW_ACTION_DROP;
    ret = flow_key_extract(mbuf, &key);
    if (ret < 0) {
        pipeline->stats.invalid_packets++;
        record_latency(&pipeline->stats, start);
        return ret;
    }

    rule = flow_table_lookup(pipeline->table, &key);
    if (rule == NULL) {
        pipeline->stats.hash_misses++;
        pipeline->stats.default_drops++;
        record_latency(&pipeline->stats, start);
        return 0;
    }

    pipeline->stats.hash_hits++;
    /* shared-readonly table 可被多个 worker 查询，计数使用 relaxed 原子累加。 */
    __atomic_fetch_add(&rule->packets, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&rule->bytes, rte_pktmbuf_pkt_len(mbuf),
                       __ATOMIC_RELAXED);
    decision->action = rule->action.type;
    decision->out_port = rule->action.out_port;
    decision->mark_id = rule->action.mark_id;
    decision->table_hit = 1;
    if (decision->action == FLOW_ACTION_DROP)
        pipeline->stats.rule_drops++;
    else if (decision->action == FLOW_ACTION_FORWARD)
        pipeline->stats.forwards++;
    else if (decision->action == FLOW_ACTION_MARK)
        pipeline->stats.marks++;
    record_latency(&pipeline->stats, start);
    return 0;
}

void flow_pipeline_print(const struct flow_pipeline *pipeline)
{
    struct flow_pipeline_stats snapshot = pipeline->stats;
    uint64_t p50 = 0;
    uint64_t p99 = 0;
    uint64_t max = 0;
    uint64_t hz = rte_get_tsc_hz();
    uint64_t p99_ns;
    uint32_t index;

    if (snapshot.latency_count > 0) {
        /* 阶段收口时离线排序，避免在 fast path 中维护高开销分位数结构。 */
        qsort(snapshot.latency_cycles, snapshot.latency_count,
              sizeof(snapshot.latency_cycles[0]), compare_u64);
        p50 = snapshot.latency_cycles[(snapshot.latency_count - 1U) * 50U / 100U];
        p99 = snapshot.latency_cycles[(snapshot.latency_count - 1U) * 99U / 100U];
        max = snapshot.latency_cycles[snapshot.latency_count - 1U];
    }
    p99_ns = hz == 0 ? 0 : p99 * UINT64_C(1000000000) / hz;
    printf("FLOW_RESULT hash_hits=%" PRIu64 " hash_misses=%" PRIu64
           " rule_drop=%" PRIu64 " forward=%" PRIu64
           " mark=%" PRIu64 " default_drop=%" PRIu64
           " invalid=%" PRIu64 "\n",
           snapshot.hash_hits, snapshot.hash_misses,
           snapshot.rule_drops, snapshot.forwards, snapshot.marks,
           snapshot.default_drops, snapshot.invalid_packets);
    printf("FLOW_LATENCY samples=%u p50_cycles=%" PRIu64
           " p99_cycles=%" PRIu64 " max_cycles=%" PRIu64
           " p99_ns=%" PRIu64 "\n",
           snapshot.latency_count, p50, p99, max,
           p99_ns);
    for (index = 0; index < pipeline->table->rule_count; ++index) {
        const struct flow_rule *rule = &pipeline->table->rules[index];

        /* filler rules没有命中，不展开打印，避免矩阵日志淹没关键结果。 */
        if (!rule->active || rule->packets == 0)
            continue;
        printf("FLOW_RULE[%u] action=%s mark=%u packets=%" PRIu64
               " bytes=%" PRIu64 "\n",
               index, flow_action_name(rule->action.type),
               rule->action.mark_id, rule->packets, rule->bytes);
    }
}

void flow_pipeline_destroy(struct flow_pipeline *pipeline)
{
    if (pipeline->owns_table)
        flow_table_destroy(pipeline->table);
    memset(pipeline, 0, sizeof(*pipeline));
}
