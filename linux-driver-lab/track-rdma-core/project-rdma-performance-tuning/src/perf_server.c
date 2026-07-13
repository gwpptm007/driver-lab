#define _POSIX_C_SOURCE 200112L
#include "rdma_cs.h"
#include "perf_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static int poll_success_quiet(struct rdma_cs_context *context, const char *tag,
                              int iteration, int log_sample)
{
    uint64_t deadline = now_ms() + RDMA_CS_CQ_TIMEOUT_MS;

    for (;;) {
        struct ibv_wc wc;
        int polled = ibv_poll_cq(context->cq, 1, &wc);

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
                              int batch_index, int count, int log_sample)
{
    uint64_t deadline = now_ms() + RDMA_CS_CQ_TIMEOUT_MS;
    int completed = 0;

    /*
     * server 侧 batch poll 的目标是确认所有 RECV WR 都被消费。
     * SEND completion 只能证明本地发送完成，RECV CQE 才能证明接收端
     * buffer 真正收到了对应消息。
     */
    while (completed < count) {
        struct ibv_wc wc[PERF_MAX_BATCH_SIZE];
        int polled = ibv_poll_cq(context->cq, count - completed, wc);

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
                                   completed + i == count - 1)) {
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
                    tag, batch_index, completed, count);
            return -1;
        }
    }

    return 0;
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

static int post_batch_recv(struct rdma_cs_context *context, int batch_index,
                           int count)
{
    struct ibv_sge sge[PERF_MAX_BATCH_SIZE];
    struct ibv_recv_wr wr[PERF_MAX_BATCH_SIZE];
    struct ibv_recv_wr *bad_wr = NULL;

    memset(sge, 0, sizeof(sge));
    memset(wr, 0, sizeof(wr));

    /*
     * RQ 也使用链表 WR 一次提交。每个 RECV 指向独立 slot，
     * 这样后续可以逐条校验 payload，确认 batch 顺序和 buffer 对应关系。
     */
    for (int i = 0; i < count; i++) {
        sge[i].addr = (uintptr_t)batch_slot(context, i);
        sge[i].length = PERF_BATCH_SLOT_SIZE;
        sge[i].lkey = context->mr->lkey;

        wr[i].wr_id = (uint64_t)(400000 + batch_index * PERF_MAX_BATCH_SIZE + i);
        wr[i].sg_list = &sge[i];
        wr[i].num_sge = 1;
        wr[i].next = i + 1 < count ? &wr[i + 1] : NULL;
    }

    if (ibv_post_recv(context->qp, &wr[0], &bad_wr) != 0) {
        fprintf(stderr, "post_batch_recv_failed batch=%d count=%d bad_wr_id=%llu\n",
                batch_index, count,
                bad_wr != NULL ? (unsigned long long)bad_wr->wr_id : 0ULL);
        return -1;
    }

    return 0;
}

static int validate_batch_payload(struct rdma_cs_context *context,
                                  int batch_index, int count)
{
    /* server 不只看 CQE 数量，还逐 slot 校验内容，确认 WR/slot 没串。 */
    for (int i = 0; i < count; i++) {
        char expected[PERF_BATCH_SLOT_SIZE];

        perf_format_batch_payload(expected, sizeof(expected),
                                  batch_index, i);
        if (strcmp(batch_slot(context, i), expected) != 0) {
            fprintf(stderr,
                    "batch_payload_mismatch batch=%d index=%d expected=%s got=%s\n",
                    batch_index, i, expected, batch_slot(context, i));
            return -1;
        }
    }

    return 0;
}

static int run_perf_server(const struct rdma_cs_options *options)
{
    struct rdma_cs_context context = {0};
    struct rdma_cs_metadata local;
    struct rdma_cs_metadata remote;
    char line[RDMA_CS_LINE_SIZE];
    int iterations = perf_get_iterations();
    int batch_size = perf_get_batch_size();
    int use_inline = perf_get_use_inline();
    int enable_rtt = perf_get_enable_rtt();
    int signal_interval = perf_get_signal_interval();
    int use_selective = perf_use_selective_signaling(signal_interval);
    const char *send_test = perf_send_test_name(use_inline);
    const char *batch_test = perf_batch_test_name(use_inline, signal_interval);
    const char *rtt_test = perf_rtt_test_name(use_inline);
    int batch_count = (iterations + batch_size - 1) / batch_size;
    int batch_messages = 0;
    int listen_fd = -1;
    int conn_fd = -1;
    int rc = EXIT_FAILURE;

    if ((size_t)batch_size * PERF_BATCH_SLOT_SIZE > RDMA_CS_BUFFER_SIZE) {
        fprintf(stderr, "batch_buffer_too_small batch_size=%d slot_size=%u buffer_size=%u\n",
                batch_size, PERF_BATCH_SLOT_SIZE, RDMA_CS_BUFFER_SIZE);
        goto out;
    }

    /*
     * 性能 server 的职责很窄：
     * 1. 建立 RDMA RC QP；
     * 2. 每轮提前 post RECV，避免 client SEND 遇到 RNR；
     * 3. 等待 RECV CQE 后通过 TCP 给 client 一个迭代完成信号。
     *
     * 注意：TCP 只用于控制节拍，不参与 RDMA 数据搬运。
     */
    rdma_cs_options_print(RDMA_CS_ROLE_SERVER,
                          use_inline ? "perf-send-latency-inline" :
                                       "perf-send-latency",
                          options);
    printf("perf_config role=server test=%s iterations=%d inline=%s\n",
           send_test, iterations, perf_inline_state(use_inline));
    printf("perf_config role=server test=%s iterations=%d batch_size=%d batches=%d inline=%s signal_mode=%s signal_interval=%d\n",
           batch_test, iterations, batch_size, batch_count,
           perf_inline_state(use_inline),
           perf_signal_mode(signal_interval), signal_interval);
    printf("perf_config role=server test=%s iterations=%d inline=%s enabled=%s\n",
           rtt_test, iterations, perf_inline_state(use_inline),
           enable_rtt ? "yes" : "no");
    perf_log_binding("server");

    puts("phase=resources_create role=server status=start");
    if (rdma_cs_resources_create(&context, options, 0x111111) != 0)
        goto out;
    puts("phase=resources_create role=server status=done");
    rdma_cs_metadata_from_context(&local, &context, RDMA_CS_ROLE_SERVER);

    listen_fd = rdma_cs_tcp_listen(options->listen_addr, options->tcp_port);
    if (listen_fd < 0)
        goto out;
    printf("server_listen=%s:%s\n", options->listen_addr, options->tcp_port);

    puts("phase=tcp_accept role=server status=waiting");
    conn_fd = rdma_cs_tcp_accept(listen_fd);
    if (conn_fd < 0 ||
        rdma_cs_exchange_metadata(conn_fd, &local, &remote) != 0)
        goto out;
    puts("phase=metadata_exchange role=server status=done");

    rdma_cs_metadata_print("server_local_metadata", &local);
    rdma_cs_metadata_print("server_remote_metadata", &remote);
    puts("TCP_CONTROL_PLANE_PASS");

    puts("phase=qp_to_rts role=server status=start");
    if (rdma_cs_qp_to_rts(&context, &remote) != 0)
        goto out;
    puts("phase=qp_to_rts role=server status=done");
    puts("RC_QP_RTS_PASS");

    for (int i = 0; i < iterations; i++) {
        /*
         * 每轮只 post 一个 RECV，便于学习 CQE 和 wr_id 的一一对应。
         * 后续 batch 项目会把这里改成一次 post 多个 RECV。
         */
        memset(context.buf, 0, RDMA_CS_BUFFER_SIZE);
        int log_sample = i == 0 || i == iterations - 1;

        if (rdma_cs_post_recv(&context, (uint64_t)(200000 + i)) != 0 ||
            rdma_cs_send_line(conn_fd, "RECV_READY\n") != 0 ||
            poll_success_quiet(&context, "server_perf_recv_cqe", i,
                               log_sample) != 0)
            goto out;

        if (strcmp(context.buf, PERF_SEND_PAYLOAD) != 0)
            goto out;

        if (rdma_cs_send_line(conn_fd, "ITER_DONE\n") != 0)
            goto out;
    }

    if (rdma_cs_recv_line(conn_fd, line, sizeof(line)) != 0 ||
        strcmp(line, "START_BATCH\n") != 0)
        goto out;

    for (int batch = 0; batch < batch_count; batch++) {
        char ready[RDMA_CS_LINE_SIZE];
        char done[RDMA_CS_LINE_SIZE];
        int remaining = iterations - batch * batch_size;
        int count = remaining < batch_size ? remaining : batch_size;
        int log_sample = batch == 0 || batch == batch_count - 1;

        memset(context.buf, 0, RDMA_CS_BUFFER_SIZE);
        snprintf(ready, sizeof(ready), "BATCH_READY count=%d\n", count);
        snprintf(done, sizeof(done), "BATCH_DONE count=%d\n", count);

        /*
         * 先 post 本批 RECV，再通知 client 可发送。
         * 这样可以把 batch WR 的实验变量限制在 SEND/RECV 链表提交方式，
         * 不把 RNR 或控制面乱序混进结果里。
         */
        if (post_batch_recv(&context, batch, count) != 0 ||
            rdma_cs_send_line(conn_fd, ready) != 0 ||
            poll_batch_success(&context, "server_perf_batch_recv_cqe",
                               batch, count, log_sample) != 0 ||
            validate_batch_payload(&context, batch, count) != 0 ||
            rdma_cs_send_line(conn_fd, done) != 0)
            goto out;

        batch_messages += count;
    }

    if (rdma_cs_recv_line(conn_fd, line, sizeof(line)) != 0)
        goto out;

    if (strcmp(line, "START_RTT\n") == 0) {
        if (!enable_rtt) {
            fprintf(stderr, "server_rtt_unexpected enable_rtt=off\n");
            goto out;
        }

        for (int i = 0; i < iterations; i++) {
            uint64_t recv_wr_id = (uint64_t)(500000 + i);
            uint64_t send_wr_id = (uint64_t)(600000 + i);
            int log_sample = i == 0 || i == iterations - 1;
            int send_flags = IBV_SEND_SIGNALED |
                             (use_inline ? IBV_SEND_INLINE : 0);

            if (post_recv_slot(&context, 0, recv_wr_id) != 0 ||
                rdma_cs_send_line(conn_fd, "RTT_READY\n") != 0 ||
                poll_success_quiet(&context, "server_perf_rtt_recv_cqe", i,
                                   log_sample) != 0)
                goto out;

            if (strcmp(batch_slot(&context, 0), PERF_RTT_REQUEST_PAYLOAD) != 0) {
                fprintf(stderr,
                        "server_rtt_request_mismatch iteration=%d expected=%s got=%s\n",
                        i, PERF_RTT_REQUEST_PAYLOAD, batch_slot(&context, 0));
                goto out;
            }

            if (rdma_cs_post_send_flags(&context, PERF_RTT_RESPONSE_PAYLOAD,
                                        send_wr_id, send_flags) != 0 ||
                poll_success_quiet(&context, "server_perf_rtt_send_cqe", i,
                                   log_sample) != 0 ||
                rdma_cs_send_line(conn_fd, "RTT_DONE\n") != 0)
                goto out;
        }

        if (rdma_cs_recv_line(conn_fd, line, sizeof(line)) != 0 ||
            strcmp(line, "PERF_DONE\n") != 0)
            goto out;
    } else if (strcmp(line, "PERF_DONE\n") != 0) {
        fprintf(stderr, "unexpected_phase_marker line=%s", line);
        goto out;
    }

    printf("perf_server_iterations=%d\n", iterations);
    printf("perf_server_batch batches=%d batch_size=%d messages=%d\n",
           batch_count, batch_size, batch_messages);
    puts(use_inline ? "PERF_SEND_LATENCY_INLINE_SERVER_PASS" :
                      "PERF_SEND_LATENCY_SERVER_PASS");
    puts(use_inline ? "PERF_BATCH_SEND_INLINE_SERVER_PASS" :
                      "PERF_BATCH_SEND_SERVER_PASS");
    if (use_selective) {
        puts(use_inline ? "PERF_BATCH_SEND_INLINE_SELECTIVE_SERVER_PASS" :
                          "PERF_BATCH_SEND_SELECTIVE_SERVER_PASS");
    }
    if (enable_rtt) {
        puts(use_inline ? "PERF_RTT_LATENCY_INLINE_SERVER_PASS" :
                          "PERF_RTT_LATENCY_SERVER_PASS");
    }
    rc = EXIT_SUCCESS;

out:
    rdma_cs_close_fd(conn_fd);
    rdma_cs_close_fd(listen_fd);
    rdma_cs_resources_destroy(&context);
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}

int main(int argc, char **argv)
{
    struct rdma_cs_options options;
    int parsed = rdma_cs_parse_common(argc, argv, &options,
                                      RDMA_CS_ROLE_SERVER);

    if (parsed > 0)
        return EXIT_SUCCESS;
    if (parsed < 0)
        return EXIT_FAILURE;

    return run_perf_server(&options);
}
