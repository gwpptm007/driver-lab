#define _POSIX_C_SOURCE 200112L
#include "rdma_cs.h"
#include "perf_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint64_t now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

struct perf_stats {
    uint64_t avg;
    uint64_t min;
    uint64_t p50;
    uint64_t p95;
    uint64_t p99;
    uint64_t max;
};

static int poll_success_quiet(struct rdma_cs_context *context, const char *tag,
                              int iteration, int log_sample,
                              int poll_budget)
{
    uint64_t deadline = now_ms() + RDMA_CS_CQ_TIMEOUT_MS;

    for (;;) {
        struct ibv_wc wc;
        int polled = ibv_poll_cq(context->cq,
                                 perf_poll_batch(1, poll_budget), &wc);

        if (polled < 0) {
            fprintf(stderr, "%s poll_failed iteration=%d\n", tag, iteration);
            return -1;
        }
        if (polled == 1) {
            if (wc.status != IBV_WC_SUCCESS) {
                fprintf(stderr,
                        "%s iteration=%d cqe_wr_id=%llu status=%s opcode=%d byte_len=%u\n",
                        tag, iteration, (unsigned long long)wc.wr_id,
                        ibv_wc_status_str(wc.status), wc.opcode, wc.byte_len);
                return -1;
            }
            if (log_sample) {
                printf("%s iteration=%d cqe_wr_id=%llu status=success opcode=%d byte_len=%u\n",
                       tag, iteration, (unsigned long long)wc.wr_id,
                       wc.opcode, wc.byte_len);
            }
            return 0;
        }
        if (now_ms() > deadline) {
            fprintf(stderr, "%s timeout iteration=%d\n", tag, iteration);
            return -1;
        }
    }
}

static int poll_batch_success(struct rdma_cs_context *context, const char *tag,
                              int batch_index, int expected_cqes,
                              int log_sample, int poll_budget)
{
    uint64_t deadline = now_ms() + RDMA_CS_CQ_TIMEOUT_MS;
    int completed = 0;

    /*
     * all-signaled 模式下，一批会有 count 个 SEND CQE；
     * selective signaling 下，只会有 expected_cqes 个 SEND CQE。
     * 这里按“预期 CQE 数”收敛，而不是按消息数收敛。
     */
    while (completed < expected_cqes) {
        struct ibv_wc wc[PERF_MAX_BATCH_SIZE];
        int remaining = expected_cqes - completed;
        int poll_batch = perf_poll_batch(remaining, poll_budget);
        int polled = ibv_poll_cq(context->cq, poll_batch, wc);

        if (polled < 0) {
            fprintf(stderr, "%s poll_failed batch=%d completed=%d\n",
                    tag, batch_index, completed);
            return -1;
        }
        if (polled > 0) {
            for (int i = 0; i < polled; i++) {
                if (wc[i].status != IBV_WC_SUCCESS) {
                    fprintf(stderr,
                            "%s batch=%d cqe_wr_id=%llu status=%s opcode=%d byte_len=%u\n",
                            tag, batch_index,
                            (unsigned long long)wc[i].wr_id,
                            ibv_wc_status_str(wc[i].status), wc[i].opcode,
                            wc[i].byte_len);
                    return -1;
                }
                if (log_sample && (completed + i == 0 ||
                                   completed + i == expected_cqes - 1)) {
                    printf("%s batch=%d cqe_index=%d cqe_wr_id=%llu status=success opcode=%d byte_len=%u\n",
                           tag, batch_index, completed + i,
                           (unsigned long long)wc[i].wr_id, wc[i].opcode,
                           wc[i].byte_len);
                }
            }
            completed += polled;
            continue;
        }
        if (now_ms() > deadline) {
            fprintf(stderr, "%s timeout batch=%d completed=%d expected=%d\n",
                    tag, batch_index, completed, expected_cqes);
            return -1;
        }
    }

    return 0;
}

static int compare_u64(const void *left, const void *right)
{
    uint64_t a = *(const uint64_t *)left;
    uint64_t b = *(const uint64_t *)right;

    return (a > b) - (a < b);
}

static uint64_t percentile(const uint64_t *values, int count, int pct)
{
    int index;

    if (count <= 0)
        return 0;
    index = (count * pct + 99) / 100 - 1;
    if (index < 0)
        index = 0;
    if (index >= count)
        index = count - 1;
    return values[index];
}

static void compute_stats(uint64_t *samples, int count, uint64_t sum,
                          struct perf_stats *stats)
{
    /* 先排序，再统一抽取均值/分位数，保证 single 与 batch 口径一致。 */
    qsort(samples, (size_t)count, sizeof(*samples), compare_u64);
    stats->avg = sum / (uint64_t)count;
    stats->min = samples[0];
    stats->p50 = percentile(samples, count, 50);
    stats->p95 = percentile(samples, count, 95);
    stats->p99 = percentile(samples, count, 99);
    stats->max = samples[count - 1];
}

static char *batch_slot(struct rdma_cs_context *context, int index)
{
    return context->buf + (size_t)index * PERF_BATCH_SLOT_SIZE;
}

static int post_recv_slot(struct rdma_cs_context *context, int slot_index,
                          uint64_t wr_id)
{
    struct ibv_sge sge = {0};
    struct ibv_recv_wr wr = {0};
    struct ibv_recv_wr *bad_wr = NULL;
    char *slot = batch_slot(context, slot_index);

    /* RTT 响应用独立 slot 接收，避免和 request SEND payload 互相覆盖。 */
    memset(slot, 0, PERF_BATCH_SLOT_SIZE);
    sge.addr = (uintptr_t)slot;
    sge.length = PERF_BATCH_SLOT_SIZE;
    sge.lkey = context->mr->lkey;

    wr.wr_id = wr_id;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    if (ibv_post_recv(context->qp, &wr, &bad_wr) != 0) {
        fprintf(stderr, "post_recv_slot_failed slot=%d wr_id=%llu bad_wr_id=%llu\n",
                slot_index, (unsigned long long)wr_id,
                bad_wr != NULL ? (unsigned long long)bad_wr->wr_id : 0ULL);
        return -1;
    }

    return 0;
}

static int post_batch_send(struct rdma_cs_context *context, int batch_index,
                           int count, int use_inline, int signal_interval)
{
    struct ibv_sge sge[PERF_MAX_BATCH_SIZE];
    struct ibv_send_wr wr[PERF_MAX_BATCH_SIZE];
    struct ibv_send_wr *bad_wr = NULL;

    memset(sge, 0, sizeof(sge));
    memset(wr, 0, sizeof(wr));

    /*
     * 每个 WR 使用 MR 里的独立 slot。这样 ibv_post_send() 返回后，
     * provider 仍可安全读取各自 payload，不会被下一条 SEND 覆盖。
     */
    for (int i = 0; i < count; i++) {
        char *slot = batch_slot(context, i);

        perf_format_batch_payload(slot, PERF_BATCH_SLOT_SIZE,
                                  batch_index, i);

        sge[i].addr = (uintptr_t)slot;
        sge[i].length = (uint32_t)strlen(slot) + 1;
        sge[i].lkey = context->mr->lkey;

        wr[i].wr_id = (uint64_t)(300000 + batch_index * PERF_MAX_BATCH_SIZE + i);
        wr[i].sg_list = &sge[i];
        wr[i].num_sge = 1;
        wr[i].opcode = IBV_WR_SEND;
        wr[i].send_flags = use_inline ? IBV_SEND_INLINE : 0;
        if (perf_wr_is_signaled(i, count, signal_interval))
            wr[i].send_flags |= IBV_SEND_SIGNALED;
        wr[i].next = i + 1 < count ? &wr[i + 1] : NULL;
    }

    if (ibv_post_send(context->qp, &wr[0], &bad_wr) != 0) {
        fprintf(stderr, "post_batch_send_failed batch=%d count=%d inline=%s signal_mode=%s signal_interval=%d bad_wr_id=%llu\n",
                batch_index, count, perf_inline_state(use_inline),
                perf_signal_mode(signal_interval), signal_interval,
                bad_wr != NULL ? (unsigned long long)bad_wr->wr_id : 0ULL);
        return -1;
    }

    return 0;
}

static int poll_rtt_success(struct rdma_cs_context *context, int iteration,
                            uint64_t send_wr_id, uint64_t recv_wr_id,
                            int log_sample, uint64_t *recv_ns)
{
    uint64_t deadline = now_ms() + RDMA_CS_CQ_TIMEOUT_MS;
    int send_done = 0;
    int recv_done = 0;

    while (!send_done || !recv_done) {
        struct ibv_wc wc[2];
        int polled = ibv_poll_cq(context->cq, 2, wc);

        if (polled < 0) {
            fprintf(stderr, "client_perf_rtt_cqe poll_failed iteration=%d\n",
                    iteration);
            return -1;
        }
        if (polled > 0) {
            for (int i = 0; i < polled; i++) {
                if (wc[i].status != IBV_WC_SUCCESS) {
                    fprintf(stderr,
                            "client_perf_rtt_cqe iteration=%d cqe_wr_id=%llu status=%s opcode=%d byte_len=%u\n",
                            iteration, (unsigned long long)wc[i].wr_id,
                            ibv_wc_status_str(wc[i].status), wc[i].opcode,
                            wc[i].byte_len);
                    return -1;
                }

                if (wc[i].wr_id == send_wr_id) {
                    send_done = 1;
                    if (log_sample) {
                        printf("client_perf_rtt_send_cqe iteration=%d cqe_wr_id=%llu status=success opcode=%d byte_len=%u\n",
                               iteration, (unsigned long long)wc[i].wr_id,
                               wc[i].opcode, wc[i].byte_len);
                    }
                    continue;
                }

                if (wc[i].wr_id == recv_wr_id) {
                    recv_done = 1;
                    *recv_ns = now_ns();
                    if (log_sample) {
                        printf("client_perf_rtt_recv_cqe iteration=%d cqe_wr_id=%llu status=success opcode=%d byte_len=%u\n",
                               iteration, (unsigned long long)wc[i].wr_id,
                               wc[i].opcode, wc[i].byte_len);
                    }
                    continue;
                }

                fprintf(stderr,
                        "client_perf_rtt_cqe unexpected_wr_id iteration=%d cqe_wr_id=%llu\n",
                        iteration, (unsigned long long)wc[i].wr_id);
                return -1;
            }
            continue;
        }

        if (now_ms() > deadline) {
            fprintf(stderr,
                    "client_perf_rtt_cqe timeout iteration=%d send_done=%d recv_done=%d\n",
                    iteration, send_done, recv_done);
            return -1;
        }
    }

    return 0;
}

static int parse_batch_ready(const char *line, int *count)
{
    /* 控制面只传本批实际条数，最后一批可能小于 batch_size。 */
    return sscanf(line, "BATCH_READY count=%d\n", count) == 1 &&
           *count > 0 && *count <= PERF_MAX_BATCH_SIZE ? 0 : -1;
}

static int expect_batch_done(const char *line, int count)
{
    int seen = 0;

    /* server 回 ACK 时必须带回同一个 count，避免 client/server 节拍错位。 */
    return sscanf(line, "BATCH_DONE count=%d\n", &seen) == 1 &&
           seen == count ? 0 : -1;
}

static int run_perf_client(const struct rdma_cs_options *options)
{
    struct rdma_cs_context context = {0};
    struct rdma_cs_metadata local;
    struct rdma_cs_metadata remote;
    char line[RDMA_CS_LINE_SIZE];
    uint64_t *samples = NULL;
    uint64_t sum = 0;
    uint64_t *batch_samples = NULL;
    uint64_t batch_sum = 0;
    uint64_t batch_total_ns = 0;
    uint64_t batch_avg_msg_ns;
    uint64_t batch_msg_per_sec;
    uint64_t *rtt_samples = NULL;
    uint64_t rtt_sum = 0;
    uint64_t rtt_overhead_x100 = 0;
    uint64_t speedup_x100;
    struct perf_stats single_stats;
    struct perf_stats batch_stats;
    struct perf_stats rtt_stats;
    int iterations = perf_get_iterations();
    int batch_size = perf_get_batch_size();
    int use_inline = perf_get_use_inline();
    int enable_rtt = perf_get_enable_rtt();
    int signal_interval = perf_get_signal_interval();
    int poll_budget = perf_get_poll_cq_budget();
    int use_selective = perf_use_selective_signaling(signal_interval);
    const char *send_test = perf_send_test_name(use_inline);
    const char *batch_test = perf_batch_test_name(use_inline, signal_interval);
    const char *rtt_test = perf_rtt_test_name(use_inline);
    int batch_count = (iterations + batch_size - 1) / batch_size;
    int batch_messages = 0;
    int batch_signaled_total = 0;
    int fd = -1;
    int rc = EXIT_FAILURE;

    if ((size_t)batch_size * PERF_BATCH_SLOT_SIZE > RDMA_CS_BUFFER_SIZE) {
        fprintf(stderr, "batch_buffer_too_small batch_size=%d slot_size=%u buffer_size=%u\n",
                batch_size, PERF_BATCH_SLOT_SIZE, RDMA_CS_BUFFER_SIZE);
        goto out;
    }

    /*
     * client 负责真正计时：
     * - 等 server 发 RECV_READY 后开始计时；
     * - post SEND；
     * - poll 到本地 SEND CQE 后停止计时；
     * - 再等 server 的 ITER_DONE，保证接收端也完成后进入下一轮。
     *
     * 因此该指标是 post_send 到本地 SEND completion 的耗时，
     * 不是完整业务 RTT，也不包含 server 侧处理耗时。
     */
    rdma_cs_options_print(RDMA_CS_ROLE_CLIENT,
                          use_inline ? "perf-send-latency-inline" :
                                       "perf-send-latency",
                          options);
    printf("perf_config role=client test=%s iterations=%d inline=%s poll_mode=%s poll_budget=%d\n",
           send_test, iterations, perf_inline_state(use_inline),
           perf_poll_mode(poll_budget), poll_budget);
    printf("perf_config role=client test=%s iterations=%d batch_size=%d batches=%d inline=%s signal_mode=%s signal_interval=%d poll_mode=%s poll_budget=%d\n",
           batch_test, iterations, batch_size, batch_count,
           perf_inline_state(use_inline),
           perf_signal_mode(signal_interval), signal_interval,
           perf_poll_mode(poll_budget), poll_budget);
    printf("perf_config role=client test=%s iterations=%d inline=%s poll_mode=%s poll_budget=%d enabled=%s\n",
           rtt_test, iterations, perf_inline_state(use_inline),
           perf_poll_mode(poll_budget), poll_budget,
           enable_rtt ? "yes" : "no");
    perf_log_binding("client");

    samples = calloc((size_t)iterations, sizeof(*samples));
    if (samples == NULL)
        goto out;
    batch_samples = calloc((size_t)batch_count, sizeof(*batch_samples));
    if (batch_samples == NULL)
        goto out;
    if (enable_rtt) {
        rtt_samples = calloc((size_t)iterations, sizeof(*rtt_samples));
        if (rtt_samples == NULL)
            goto out;
    }

    puts("phase=resources_create role=client status=start");
    if (rdma_cs_resources_create(&context, options, 0x222222) != 0)
        goto out;
    puts("phase=resources_create role=client status=done");
    rdma_cs_metadata_from_context(&local, &context, RDMA_CS_ROLE_CLIENT);

    puts("phase=tcp_connect role=client status=start");
    fd = rdma_cs_tcp_connect(options->server_addr, options->tcp_port);
    if (fd < 0 ||
        rdma_cs_exchange_metadata(fd, &local, &remote) != 0)
        goto out;
    puts("phase=metadata_exchange role=client status=done");

    rdma_cs_metadata_print("client_local_metadata", &local);
    rdma_cs_metadata_print("client_remote_metadata", &remote);
    puts("TCP_CONTROL_PLANE_PASS");

    puts("phase=qp_to_rts role=client status=start");
    if (rdma_cs_qp_to_rts(&context, &remote) != 0)
        goto out;
    puts("phase=qp_to_rts role=client status=done");
    puts("RC_QP_RTS_PASS");

    for (int i = 0; i < iterations; i++) {
        uint64_t start;
        uint64_t end;

        /* 单条路径保持最朴素的节拍，作为 batch/inline 的对照基线。 */
        if (rdma_cs_recv_line(fd, line, sizeof(line)) != 0 ||
            strcmp(line, "RECV_READY\n") != 0)
            goto out;

        int log_sample = i == 0 || i == iterations - 1;

        start = now_ns();
        if (rdma_cs_post_send_flags(&context, PERF_SEND_PAYLOAD,
                                    (uint64_t)(100000 + i),
                                    IBV_SEND_SIGNALED |
                                        (use_inline ? IBV_SEND_INLINE : 0)) != 0 ||
            poll_success_quiet(&context, "client_perf_send_cqe", i,
                               log_sample, poll_budget) != 0)
            goto out;
        end = now_ns();

        samples[i] = end - start;
        sum += samples[i];

        if (rdma_cs_recv_line(fd, line, sizeof(line)) != 0 ||
            strcmp(line, "ITER_DONE\n") != 0)
            goto out;
    }

    compute_stats(samples, iterations, sum, &single_stats);

    printf("perf_result test=%s iterations=%d inline=%s poll_mode=%s poll_budget=%d avg_ns=%llu "
           "min_ns=%llu p50_ns=%llu p95_ns=%llu p99_ns=%llu max_ns=%llu\n",
           send_test, iterations, perf_inline_state(use_inline),
           perf_poll_mode(poll_budget), poll_budget,
           (unsigned long long)single_stats.avg,
           (unsigned long long)single_stats.min,
           (unsigned long long)single_stats.p50,
           (unsigned long long)single_stats.p95,
           (unsigned long long)single_stats.p99,
           (unsigned long long)single_stats.max);

    if (rdma_cs_send_line(fd, "START_BATCH\n") != 0)
        goto out;

    for (int batch = 0; batch < batch_count; batch++) {
        uint64_t start;
        uint64_t end;
        int count = 0;
        int signaled_cqes = 0;
        int log_sample = batch == 0 || batch == batch_count - 1;

        if (rdma_cs_recv_line(fd, line, sizeof(line)) != 0 ||
            parse_batch_ready(line, &count) != 0)
            goto out;

        signaled_cqes = perf_count_signaled(count, signal_interval);
        if (log_sample) {
            printf("batch_signal_plan batch=%d count=%d signal_mode=%s signal_interval=%d signaled_cqes=%d\n",
                   batch, count, perf_signal_mode(signal_interval),
                   signal_interval, signaled_cqes);
        }

        /*
         * batch latency 的定义是：
         * 从链表 WR 提交前，到本批“应出现的 SEND CQE”全部收齐为止。
         * selective signaling 下，批尾 WR 一定 signaled，因此最后一个
         * CQE 仍然可以作为整批完成的时间锚点。
         * 再用总耗时 / 总消息数折算出 avg_msg_ns，便于和 single 对比。
         */
        start = now_ns();
        if (post_batch_send(&context, batch, count, use_inline,
                            signal_interval) != 0 ||
            poll_batch_success(&context, "client_perf_batch_send_cqe",
                               batch, signaled_cqes, log_sample,
                               poll_budget) != 0)
            goto out;
        end = now_ns();

        batch_samples[batch] = end - start;
        batch_sum += batch_samples[batch];
        batch_total_ns += batch_samples[batch];
        batch_messages += count;
        batch_signaled_total += signaled_cqes;

        if (rdma_cs_recv_line(fd, line, sizeof(line)) != 0 ||
            expect_batch_done(line, count) != 0)
            goto out;
    }

    compute_stats(batch_samples, batch_count, batch_sum, &batch_stats);
    /* 吞吐与 speedup 都基于整个 batch 阶段总量计算，避免只看单批抖动。 */
    batch_avg_msg_ns = batch_total_ns > 0 ?
                           batch_total_ns / (uint64_t)batch_messages : 0;
    batch_msg_per_sec = batch_total_ns > 0 ?
                            (uint64_t)batch_messages * 1000000000ULL /
                                batch_total_ns : 0;
    speedup_x100 = batch_avg_msg_ns > 0 ?
                       single_stats.avg * 100ULL / batch_avg_msg_ns : 0;

    printf("perf_result test=%s batches=%d batch_size=%d messages=%d inline=%s "
           "signal_mode=%s signal_interval=%d signaled_total=%d "
           "poll_mode=%s poll_budget=%d "
           "avg_batch_ns=%llu avg_msg_ns=%llu min_batch_ns=%llu "
           "p50_batch_ns=%llu p95_batch_ns=%llu p99_batch_ns=%llu max_batch_ns=%llu\n",
           batch_test, batch_count, batch_size, batch_messages,
           perf_inline_state(use_inline),
           perf_signal_mode(signal_interval), signal_interval,
           batch_signaled_total,
           perf_poll_mode(poll_budget), poll_budget,
           (unsigned long long)batch_stats.avg,
           (unsigned long long)batch_avg_msg_ns,
           (unsigned long long)batch_stats.min,
           (unsigned long long)batch_stats.p50,
           (unsigned long long)batch_stats.p95,
           (unsigned long long)batch_stats.p99,
           (unsigned long long)batch_stats.max);
    printf("perf_throughput test=%s messages=%d total_ns=%llu inline=%s signal_mode=%s signal_interval=%d poll_mode=%s poll_budget=%d msg_per_sec=%llu\n",
           batch_test, batch_messages, (unsigned long long)batch_total_ns,
           perf_inline_state(use_inline),
           perf_signal_mode(signal_interval), signal_interval,
           perf_poll_mode(poll_budget), poll_budget,
           (unsigned long long)batch_msg_per_sec);
    printf("perf_compare single_vs_batch inline=%s signal_mode=%s signal_interval=%d signaled_total=%d poll_mode=%s poll_budget=%d single_avg_ns=%llu batch_avg_msg_ns=%llu speedup_x100=%llu\n",
           perf_inline_state(use_inline),
           perf_signal_mode(signal_interval), signal_interval,
           batch_signaled_total,
           perf_poll_mode(poll_budget), poll_budget,
           (unsigned long long)single_stats.avg,
           (unsigned long long)batch_avg_msg_ns,
           (unsigned long long)speedup_x100);

    if (enable_rtt) {
        if (rdma_cs_send_line(fd, "START_RTT\n") != 0)
            goto out;

        for (int i = 0; i < iterations; i++) {
            uint64_t start;
            uint64_t response_ns = 0;
            uint64_t send_wr_id = (uint64_t)(710000 + i);
            uint64_t recv_wr_id = (uint64_t)(700000 + i);
            int log_sample = i == 0 || i == iterations - 1;
            int send_flags = IBV_SEND_SIGNALED |
                             (use_inline ? IBV_SEND_INLINE : 0);

            if (rdma_cs_recv_line(fd, line, sizeof(line)) != 0 ||
                strcmp(line, "RTT_READY\n") != 0)
                goto out;

            if (post_recv_slot(&context, 1, recv_wr_id) != 0)
                goto out;

            start = now_ns();
            if (rdma_cs_post_send_flags(&context, PERF_RTT_REQUEST_PAYLOAD,
                                        send_wr_id, send_flags) != 0 ||
                poll_rtt_success(&context, i, send_wr_id, recv_wr_id,
                                 log_sample, &response_ns) != 0)
                goto out;

            rtt_samples[i] = response_ns - start;
            rtt_sum += rtt_samples[i];

            if (strcmp(batch_slot(&context, 1), PERF_RTT_RESPONSE_PAYLOAD) != 0) {
                fprintf(stderr,
                        "client_rtt_response_mismatch iteration=%d expected=%s got=%s\n",
                        i, PERF_RTT_RESPONSE_PAYLOAD, batch_slot(&context, 1));
                goto out;
            }

            if (rdma_cs_recv_line(fd, line, sizeof(line)) != 0 ||
                strcmp(line, "RTT_DONE\n") != 0)
                goto out;
        }

        compute_stats(rtt_samples, iterations, rtt_sum, &rtt_stats);
        rtt_overhead_x100 = single_stats.avg > 0 ?
                            rtt_stats.avg * 100ULL / single_stats.avg : 0;

        printf("perf_result test=%s iterations=%d inline=%s poll_mode=%s poll_budget=%d avg_ns=%llu "
               "min_ns=%llu p50_ns=%llu p95_ns=%llu p99_ns=%llu max_ns=%llu\n",
               rtt_test, iterations, perf_inline_state(use_inline),
               perf_poll_mode(poll_budget), poll_budget,
               (unsigned long long)rtt_stats.avg,
               (unsigned long long)rtt_stats.min,
               (unsigned long long)rtt_stats.p50,
               (unsigned long long)rtt_stats.p95,
               (unsigned long long)rtt_stats.p99,
               (unsigned long long)rtt_stats.max);
        printf("perf_compare send_vs_rtt inline=%s poll_mode=%s poll_budget=%d send_avg_ns=%llu rtt_avg_ns=%llu rtt_overhead_x100=%llu\n",
               perf_inline_state(use_inline),
               perf_poll_mode(poll_budget), poll_budget,
               (unsigned long long)single_stats.avg,
               (unsigned long long)rtt_stats.avg,
               (unsigned long long)rtt_overhead_x100);
    }

    if (rdma_cs_send_line(fd, "PERF_DONE\n") != 0)
        goto out;

    puts(use_inline ? "PERF_SEND_LATENCY_INLINE_CLIENT_PASS" :
                      "PERF_SEND_LATENCY_CLIENT_PASS");
    puts(use_inline ? "PERF_BATCH_SEND_INLINE_CLIENT_PASS" :
                      "PERF_BATCH_SEND_CLIENT_PASS");
    if (use_selective) {
        puts(use_inline ? "PERF_BATCH_SEND_INLINE_SELECTIVE_CLIENT_PASS" :
                          "PERF_BATCH_SEND_SELECTIVE_CLIENT_PASS");
    }
    if (enable_rtt) {
        puts(use_inline ? "PERF_RTT_LATENCY_INLINE_CLIENT_PASS" :
                          "PERF_RTT_LATENCY_CLIENT_PASS");
    }
    rc = EXIT_SUCCESS;

out:
    free(rtt_samples);
    free(batch_samples);
    free(samples);
    rdma_cs_close_fd(fd);
    rdma_cs_resources_destroy(&context);
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}

int main(int argc, char **argv)
{
    struct rdma_cs_options options;
    int parsed = rdma_cs_parse_common(argc, argv, &options,
                                      RDMA_CS_ROLE_CLIENT);

    if (parsed > 0)
        return EXIT_SUCCESS;
    if (parsed < 0)
        return EXIT_FAILURE;

    return run_perf_client(&options);
}
