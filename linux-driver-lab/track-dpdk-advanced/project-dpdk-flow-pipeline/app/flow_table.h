#ifndef FLOW_TABLE_H
#define FLOW_TABLE_H

#include <stdint.h>
#include <rte_hash.h>

#include "flow_key.h"

#define FLOW_TABLE_MAX_RULES 1024U

/* miss 不进入该枚举，由 pipeline 直接执行 default DROP。 */
enum flow_action_type {
    FLOW_ACTION_DROP = 0,
    FLOW_ACTION_FORWARD,
    FLOW_ACTION_MARK,
};

struct flow_action {
    enum flow_action_type type;
    uint16_t out_port;
    uint32_t mark_id;
};

struct flow_rule {
    struct flow_key key;
    struct flow_action action;
    uint64_t packets;
    uint64_t bytes;
    uint64_t last_seen_tsc;
    uint32_t generation;
    uint8_t active;
    uint8_t dynamic;
};

/* rte_hash 保存 key 到 rules[] 稳定地址的映射，数组槽位支持删除后复用。 */
struct flow_table {
    struct rte_hash *hash;
    struct flow_rule rules[FLOW_TABLE_MAX_RULES];
    uint32_t rule_count;
    uint32_t next_generation;
};

/* create 同时装载三条静态基准规则，destroy 释放 rte_hash。 */
int flow_table_create(struct flow_table *table, uint16_t out_port);
struct flow_rule *flow_table_lookup(struct flow_table *table,
                                    const struct flow_key *key);
int flow_table_upsert(struct flow_table *table, const struct flow_key *key,
                      enum flow_action_type type, uint16_t out_port,
                      uint32_t mark_id, int dynamic);
int flow_table_add_filler_rules(struct flow_table *table, uint32_t count,
                                uint16_t out_port);
/* delete/age 先删除 hash key，再回收逻辑槽位，避免 lookup 悬空指针。 */
int flow_table_delete(struct flow_table *table, const struct flow_key *key);
uint32_t flow_table_age(struct flow_table *table, uint64_t now_tsc,
                        uint64_t timeout_cycles);
int flow_table_lifecycle_selftest(struct flow_table *table);
void flow_table_destroy(struct flow_table *table);
const char *flow_action_name(enum flow_action_type action);

#endif
