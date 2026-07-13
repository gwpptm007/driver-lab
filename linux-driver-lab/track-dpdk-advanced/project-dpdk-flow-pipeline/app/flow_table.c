#include "flow_table.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rte_jhash.h>
#include <rte_lcore.h>
#include <rte_errno.h>
#include <rte_cycles.h>

static uint32_t table_sequence;

static int flow_table_add(struct flow_table *table,
                          const struct flow_key *key,
                          enum flow_action_type type,
                          uint16_t out_port, uint32_t mark_id)
{
    struct flow_rule *rule;
    uint32_t index;
    int ret;

    /* 优先复用已删除或 aging 回收的槽位，避免高水位持续增长。 */
    for (index = 0; index < table->rule_count; ++index) {
        if (!table->rules[index].active)
            break;
    }
    if (index == table->rule_count) {
        if (table->rule_count >= FLOW_TABLE_MAX_RULES)
            return -ENOSPC;
        table->rule_count++;
    }
    rule = &table->rules[index];
    memset(rule, 0, sizeof(*rule));
    rule->key = *key;
    rule->action.type = type;
    rule->action.out_port = out_port;
    rule->action.mark_id = mark_id;
    rule->active = 1;
    rule->generation = ++table->next_generation;
    ret = rte_hash_add_key_data(table->hash, &rule->key, rule);
    if (ret < 0)
        return ret;
    return 0;
}

int flow_table_create(struct flow_table *table, uint16_t out_port)
{
    struct rte_hash_parameters params;
    struct flow_key key;
    char name[RTE_HASH_NAMESIZE];

    memset(table, 0, sizeof(*table));
    memset(&params, 0, sizeof(params));
    snprintf(name, sizeof(name), "flow_%ld_%u", (long)getpid(),
             table_sequence++);
    params.name = name;
    params.entries = 2048;
    params.key_len = sizeof(struct flow_key);
    params.hash_func = rte_jhash;
    params.hash_func_init_val = 0;
    params.socket_id = rte_socket_id();
    table->hash = rte_hash_create(&params);
    if (table->hash == NULL)
        return -rte_errno;
    puts("FLOW_HASH_CREATE_PASS");

    /* 三条精确五元组规则分别覆盖 DROP、FORWARD 和 MARK。 */
    flow_key_set_ipv4_udp(&key, 10, 1, 0, 1, 10, 20, 0, 1,
                          10001, 20001);
    if (flow_table_add(table, &key, FLOW_ACTION_DROP, out_port, 0) < 0)
        goto fail;
    flow_key_set_ipv4_udp(&key, 10, 1, 0, 2, 10, 20, 0, 2,
                          10002, 20002);
    if (flow_table_add(table, &key, FLOW_ACTION_MARK, out_port, 42) < 0)
        goto fail;
    flow_key_set_ipv4_udp(&key, 10, 1, 0, 3, 10, 20, 0, 3,
                          10003, 20003);
    if (flow_table_add(table, &key, FLOW_ACTION_FORWARD, out_port, 0) < 0)
        goto fail;

    printf("FLOW_RULE_LOAD_PASS count=%u\n", table->rule_count);
    return 0;

fail:
    flow_table_destroy(table);
    return -EINVAL;
}

struct flow_rule *flow_table_lookup(struct flow_table *table,
                                    const struct flow_key *key)
{
    void *data = NULL;

    if (rte_hash_lookup_data(table->hash, key, &data) < 0)
        return NULL;
    if (((struct flow_rule *)data)->dynamic)
        __atomic_store_n(&((struct flow_rule *)data)->last_seen_tsc,
                         rte_rdtsc(), __ATOMIC_RELAXED);
    return data;
}

int flow_table_upsert(struct flow_table *table, const struct flow_key *key,
                      enum flow_action_type type, uint16_t out_port,
                      uint32_t mark_id, int dynamic)
{
    struct flow_rule *rule = flow_table_lookup(table, key);

    if (rule != NULL) {
        /* key 不变时原位更新 action，rte_hash 中的数据指针保持稳定。 */
        rule->action.type = type;
        rule->action.out_port = out_port;
        rule->action.mark_id = mark_id;
        rule->dynamic = dynamic != 0;
        rule->generation = ++table->next_generation;
        rule->last_seen_tsc = rte_rdtsc();
        return 0;
    }
    if (flow_table_add(table, key, type, out_port, mark_id) < 0)
        return -1;
    rule = flow_table_lookup(table, key);
    if (rule == NULL)
        return -1;
    rule->dynamic = dynamic != 0;
    rule->last_seen_tsc = rte_rdtsc();
    return 1;
}

int flow_table_delete(struct flow_table *table, const struct flow_key *key)
{
    struct flow_rule *rule = flow_table_lookup(table, key);

    if (rule == NULL)
        return -ENOENT;
    if (rte_hash_del_key(table->hash, key) < 0)
        return -EIO;
    rule->active = 0;
    return 0;
}

int flow_table_add_filler_rules(struct flow_table *table, uint32_t count,
                                uint16_t out_port)
{
    uint32_t index;

    if (count > FLOW_TABLE_MAX_RULES - table->rule_count)
        return -ENOSPC;
    for (index = 0; index < count; ++index) {
        struct flow_key key;
        uint8_t third = (uint8_t)(index / 250U);
        uint8_t fourth = (uint8_t)(index % 250U + 1U);

        /* filler key 位于独立地址段，不会命中 pcap 中的测试流。 */
        flow_key_set_ipv4_udp(&key, 172, 16, third, fourth,
                              192, 0, third, fourth,
                              (uint16_t)(30000U + index % 10000U),
                              (uint16_t)(40000U + index % 10000U));
        if (flow_table_upsert(table, &key, FLOW_ACTION_FORWARD,
                              out_port, 0, 0) != 1)
            return -1;
    }
    printf("FLOW_FILLER_RULES_PASS extra=%u total=%u\n",
           count, table->rule_count);
    return 0;
}

uint32_t flow_table_age(struct flow_table *table, uint64_t now_tsc,
                        uint64_t timeout_cycles)
{
    uint32_t aged = 0;
    uint32_t index;

    for (index = 0; index < table->rule_count; ++index) {
        struct flow_rule *rule = &table->rules[index];

        if (!rule->active || !rule->dynamic || now_tsc < rule->last_seen_tsc ||
            now_tsc - rule->last_seen_tsc < timeout_cycles)
            continue;
        if (rte_hash_del_key(table->hash, &rule->key) >= 0) {
            rule->active = 0;
            aged++;
        }
    }
    return aged;
}

int flow_table_lifecycle_selftest(struct flow_table *table)
{
    struct flow_key key;
    struct flow_rule *rule;
    uint64_t synthetic_now = UINT64_C(10000);
    uint32_t generation;

    flow_key_set_ipv4_udp(&key, 10, 1, 0, 5, 10, 20, 0, 5,
                          10005, 20005);
    if (flow_table_upsert(table, &key, FLOW_ACTION_FORWARD, 1, 0, 1) != 1)
        return -1;
    rule = flow_table_lookup(table, &key);
    if (rule == NULL || rule->action.type != FLOW_ACTION_FORWARD)
        return -1;
    generation = rule->generation;
    if (flow_table_upsert(table, &key, FLOW_ACTION_MARK, 1, 77, 1) != 0)
        return -1;
    rule = flow_table_lookup(table, &key);
    if (rule == NULL || rule->action.type != FLOW_ACTION_MARK ||
        rule->action.mark_id != 77 || rule->generation <= generation)
        return -1;
    if (flow_table_delete(table, &key) != 0 ||
        flow_table_lookup(table, &key) != NULL)
        return -1;
    puts("FLOW_RULE_ADD_UPDATE_DELETE_PASS");

    flow_key_set_ipv4_udp(&key, 10, 1, 0, 6, 10, 20, 0, 6,
                          10006, 20006);
    if (flow_table_upsert(table, &key, FLOW_ACTION_DROP, 1, 0, 1) != 1)
        return -1;
    rule = flow_table_lookup(table, &key);
    if (rule == NULL)
        return -1;
    /* 使用合成时间构造确定性超时，不依赖测试机调度抖动。 */
    rule->last_seen_tsc = synthetic_now - 100;
    if (flow_table_age(table, synthetic_now, 50) != 1 ||
        flow_table_lookup(table, &key) != NULL)
        return -1;
    puts("FLOW_RULE_AGING_PASS");
    puts("DPDK_FLOW_PIPELINE_PHASE2_PASS");
    return 0;
}

void flow_table_destroy(struct flow_table *table)
{
    if (table->hash != NULL)
        rte_hash_free(table->hash);
    memset(table, 0, sizeof(*table));
}

const char *flow_action_name(enum flow_action_type action)
{
    switch (action) {
    case FLOW_ACTION_DROP:
        return "DROP";
    case FLOW_ACTION_FORWARD:
        return "FORWARD";
    case FLOW_ACTION_MARK:
        return "MARK";
    default:
        return "UNKNOWN";
    }
}
