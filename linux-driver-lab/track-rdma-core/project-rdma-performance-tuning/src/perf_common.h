#ifndef RDMA_PERF_COMMON_H
#define RDMA_PERF_COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PERF_DEFAULT_ITERATIONS 1000
#define PERF_DEFAULT_BATCH_SIZE 8
#define PERF_DEFAULT_SIGNAL_INTERVAL 1
#define PERF_MAX_BATCH_SIZE 16
#define PERF_DEFAULT_POLL_CQ_BUDGET PERF_MAX_BATCH_SIZE
#define PERF_BATCH_SLOT_SIZE 64U
#define PERF_SEND_PAYLOAD "rdma-perf-send"
#define PERF_BATCH_PAYLOAD_PREFIX "b"
/*
 * RTT phase 也要兼容 inline 模式，因此 payload 保持极短，
 * 避免在 RXE 上再次撞到 max_inline_data 上限。
 */
#define PERF_RTT_REQUEST_PAYLOAD "rttq"
#define PERF_RTT_RESPONSE_PAYLOAD "rttr"

static inline const char *perf_env_or_dash(const char *name)
{
    const char *value = getenv(name);

    return value != NULL && value[0] != '\0' ? value : "-";
}

static inline const char *perf_role_cpuset_env(const char *role)
{
    return strcmp(role, "server") == 0 ?
           "PERF_SERVER_CPUSET" : "PERF_CLIENT_CPUSET";
}

static inline const char *perf_role_numa_env(const char *role)
{
    return strcmp(role, "server") == 0 ?
           "PERF_SERVER_NUMA_NODE" : "PERF_CLIENT_NUMA_NODE";
}

static inline int perf_parse_positive_env(const char *name, int fallback,
                                          int upper_bound)
{
    const char *value = getenv(name);
    char *end = NULL;
    long parsed;

    if (value == NULL || value[0] == '\0')
        return fallback;

    parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0 ||
        parsed > upper_bound)
        return fallback;

    return (int)parsed;
}

static inline int perf_get_iterations(void)
{
    return perf_parse_positive_env("PERF_ITERATIONS",
                                   PERF_DEFAULT_ITERATIONS, 10000000);
}

static inline int perf_get_batch_size(void)
{
    /*
     * 上一阶段公共 helper 里 QP 的 max_send_wr/max_recv_wr 都是 16。
     * batch 阶段先把默认值控制在这个范围内，避免学习实验因为 SQ/RQ
     * 深度不足而失败；后续如果要测更大 batch，应先扩 QP cap。
     */
    return perf_parse_positive_env("PERF_BATCH_SIZE",
                                   PERF_DEFAULT_BATCH_SIZE,
                                   PERF_MAX_BATCH_SIZE);
}

static inline int perf_get_use_inline(void)
{
    const char *value = getenv("PERF_USE_INLINE");

    if (value == NULL || value[0] == '\0')
        return 0;

    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "on") == 0 ||
           strcmp(value, "yes") == 0;
}

static inline int perf_get_enable_rtt(void)
{
    const char *value = getenv("PERF_ENABLE_RTT");

    if (value == NULL || value[0] == '\0')
        return 0;

    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "on") == 0 ||
           strcmp(value, "yes") == 0;
}

static inline int perf_get_signal_interval(void)
{
    /*
     * selective signaling 只在 batch SEND 路径使用。默认 1 表示每条 WR
     * 都 signaled；大于 1 时表示每 N 条 WR 才产生一个 SEND CQE，同时
     * 批尾 WR 一定 signaled，保证整批完成有可观测锚点。
     */
    return perf_parse_positive_env("PERF_SIGNAL_INTERVAL",
                                   PERF_DEFAULT_SIGNAL_INTERVAL,
                                   PERF_MAX_BATCH_SIZE);
}

static inline int perf_get_poll_cq_budget(void)
{
    /*
     * 这里控制的是 client 每次 ibv_poll_cq() 最多取回多少个 CQE。
     * 默认值给到 max batch size，等价于当前 batch 路径“尽快把本轮 CQE
     * 一次取完”的行为；只有显式设成 1/2/4/8 时，才进入 polling 对比模式。
     */
    return perf_parse_positive_env("PERF_POLL_CQ_BUDGET",
                                   PERF_DEFAULT_POLL_CQ_BUDGET,
                                   PERF_MAX_BATCH_SIZE);
}

static inline int perf_use_selective_signaling(int signal_interval)
{
    return signal_interval > 1;
}

static inline const char *perf_signal_mode(int signal_interval)
{
    return perf_use_selective_signaling(signal_interval) ?
           "selective" : "all";
}

static inline const char *perf_poll_mode(int poll_budget)
{
    return poll_budget == 1 ? "single" : "burst";
}

static inline int perf_poll_batch(int remaining, int poll_budget)
{
    return remaining < poll_budget ? remaining : poll_budget;
}

static inline int perf_wr_is_signaled(int index, int count, int signal_interval)
{
    if (!perf_use_selective_signaling(signal_interval))
        return 1;
    return ((index + 1) % signal_interval) == 0 || index == count - 1;
}

static inline int perf_count_signaled(int wr_count, int signal_interval)
{
    if (!perf_use_selective_signaling(signal_interval))
        return wr_count;
    return (wr_count + signal_interval - 1) / signal_interval;
}

static inline const char *perf_inline_state(int use_inline)
{
    return use_inline ? "on" : "off";
}

static inline const char *perf_send_test_name(int use_inline)
{
    return use_inline ? "send_latency_inline" : "send_latency";
}

static inline const char *perf_batch_test_name(int use_inline,
                                               int signal_interval)
{
    if (use_inline && perf_use_selective_signaling(signal_interval))
        return "batch_send_inline_selective";
    if (use_inline)
        return "batch_send_inline";
    if (perf_use_selective_signaling(signal_interval))
        return "batch_send_selective";
    return "batch_send";
}

static inline const char *perf_rtt_test_name(int use_inline)
{
    return use_inline ? "rtt_latency_inline" : "rtt_latency";
}

static inline int perf_read_proc_status_value(const char *key, char *buf,
                                              size_t buf_size)
{
#ifdef __linux__
    FILE *fp = fopen("/proc/self/status", "r");
    char line[256];
    size_t key_len = strlen(key);

    if (fp == NULL) {
        snprintf(buf, buf_size, "unavailable");
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == ':') {
            char *value = line + key_len + 1;

            while (*value == ' ' || *value == '\t')
                value++;
            snprintf(buf, buf_size, "%s", value);
            buf[strcspn(buf, "\r\n")] = '\0';
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
#endif
    snprintf(buf, buf_size, "unavailable");
    return -1;
}

static inline int perf_current_cpu(void)
{
#ifdef __linux__
    FILE *fp = fopen("/proc/self/stat", "r");
    char line[1024];
    char *cursor;
    int field = 0;

    if (fp == NULL)
        return -1;
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    cursor = strrchr(line, ')');
    if (cursor == NULL || cursor[1] != ' ')
        return -1;
    cursor += 2;

    for (char *token = strtok(cursor, " "); token != NULL;
         token = strtok(NULL, " ")) {
        field++;
        if (field == 37)
            return (int)strtol(token, NULL, 10);
    }
#endif
    return -1;
}

static inline void perf_log_binding(const char *role)
{
    char cpus_allowed[128];
    char mems_allowed[128];

    perf_read_proc_status_value("Cpus_allowed_list", cpus_allowed,
                                sizeof(cpus_allowed));
    perf_read_proc_status_value("Mems_allowed_list", mems_allowed,
                                sizeof(mems_allowed));

    printf("perf_binding role=%s requested_cpuset=%s requested_numa_node=%s current_cpu=%d cpus_allowed=%s mems_allowed=%s\n",
           role, perf_env_or_dash(perf_role_cpuset_env(role)),
           perf_env_or_dash(perf_role_numa_env(role)),
           perf_current_cpu(), cpus_allowed, mems_allowed);
}

static inline void perf_format_batch_payload(char *buf, size_t buf_size,
                                             int batch_index, int wr_index)
{
    /*
     * batch inline 在 RXE 上很容易踩 max_inline_data 上限。
     * 这里统一使用紧凑编码，既保留 batch/index 可校验信息，
     * 又尽量把 payload 压到单条 inline SEND 已验证可工作的长度以内。
     */
    snprintf(buf, buf_size, "%s%x-%x", PERF_BATCH_PAYLOAD_PREFIX,
             batch_index, wr_index);
}

#endif
