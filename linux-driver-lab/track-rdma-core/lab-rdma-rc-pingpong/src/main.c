#define _POSIX_C_SOURCE 200112L
#include "pingpong.h"
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static struct ibv_qp *create_qp(struct session *session)
{
    struct ibv_qp_init_attr attr = {0};

    /* SQ 和 RQ 共用一个 CQ，后续通过 CQE opcode/wr_id 区分完成类型。 */
    attr.send_cq = session->cq;
    attr.recv_cq = session->cq;
    attr.qp_type = IBV_QPT_RC;
    attr.cap.max_send_wr = 16;
    attr.cap.max_recv_wr = 16;
    attr.cap.max_send_sge = 1;
    attr.cap.max_recv_sge = 1;

    return ibv_create_qp(session->pd, &attr);
}

static int to_init(struct session *session, struct endpoint *endpoint)
{
    struct ibv_qp_attr attr = {0};

    /* INIT 确定本地 port、P_Key 以及该 QP 允许的远端访问能力。 */
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = session->port;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                           IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;

    return ibv_modify_qp(endpoint->qp, &attr,
                         IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                         IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
}

static int to_rtr(struct session *session, struct endpoint *endpoint,
                  struct endpoint *peer)
{
    struct ibv_qp_attr attr = {0};

    /*
     * RTR 描述接收方向：对端 QPN/PSN、路径 MTU 和 RoCE GID。
     * rq_psn 必须填写“对端将从哪个 PSN 开始发送”。
     */
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = session->port_attr.active_mtu;
    attr.dest_qp_num = peer->qp->qp_num;
    attr.rq_psn = peer->psn;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 12;
    /* RoCE 使用 Global Route Header/GID，不能依赖 InfiniBand LID 路由。 */
    attr.ah_attr.is_global = 1;
    attr.ah_attr.port_num = session->port;
    attr.ah_attr.grh.dgid = session->gid;
    attr.ah_attr.grh.sgid_index = session->gid_index;
    attr.ah_attr.grh.hop_limit = 1;

    return ibv_modify_qp(endpoint->qp, &attr,
                         IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU |
                         IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
                         IBV_QP_MAX_DEST_RD_ATOMIC |
                         IBV_QP_MIN_RNR_TIMER);
}

static int to_rts(struct endpoint *endpoint)
{
    struct ibv_qp_attr attr = {0};

    /* RTS 描述发送方向：本端 SQ PSN、超时、重试和 outstanding RDMA read。 */
    attr.qp_state = IBV_QPS_RTS;
    attr.sq_psn = endpoint->psn;
    attr.timeout = 14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.max_rd_atomic = 1;

    return ibv_modify_qp(endpoint->qp, &attr,
                         IBV_QP_STATE | IBV_QP_TIMEOUT |
                         IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                         IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
}

static int setup_endpoint(struct session *session, struct endpoint *endpoint,
                          const char *name, uint32_t psn)
{
    endpoint->name = name;
    endpoint->psn = psn;
    /* 每个端点使用独立 buffer/MR，避免 ping 和 pong 相互覆盖资源归属。 */
    if (posix_memalign((void **)&endpoint->buf, 4096, BUFFER_SIZE) != 0)
        return -1;

    memset(endpoint->buf, 0, BUFFER_SIZE);
    /* 接收操作会由设备写入 buffer，因此 MR 必须包含 LOCAL_WRITE。 */
    endpoint->mr = ibv_reg_mr(session->pd, endpoint->buf, BUFFER_SIZE,
                              IBV_ACCESS_LOCAL_WRITE);
    endpoint->qp = create_qp(session);

    return endpoint->mr != NULL && endpoint->qp != NULL ? 0 : -1;
}

/* Receive WR 必须先进入 RQ，否则 SEND 到达时会触发 RNR。 */
static int post_recv(struct endpoint *endpoint, uint64_t wr_id)
{
    struct ibv_sge sge = {
        .addr = (uintptr_t)endpoint->buf,
        .length = BUFFER_SIZE,
        .lkey = endpoint->mr->lkey,
    };
    /* wr_id 完全由应用定义，完成后会原样出现在对应 CQE 中。 */
    struct ibv_recv_wr wr = {
        .wr_id = wr_id,
        .sg_list = &sge,
        .num_sge = 1,
    };
    struct ibv_recv_wr *bad_wr = NULL;

    return ibv_post_recv(endpoint->qp, &wr, &bad_wr);
}

static int post_send(struct endpoint *endpoint, const char *text, uint64_t wr_id)
{
    struct ibv_sge sge;
    struct ibv_send_wr wr = {0};
    struct ibv_send_wr *bad_wr = NULL;

    /* SGE 用 address/length/lkey 描述本地已注册内存。 */
    snprintf(endpoint->buf, BUFFER_SIZE, "%s", text);
    sge.addr = (uintptr_t)endpoint->buf;
    sge.length = (uint32_t)strlen(endpoint->buf) + 1;
    sge.lkey = endpoint->mr->lkey;
    wr.wr_id = wr_id;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_SEND;
    /* 显式请求 CQE，否则成功 SEND 可能不会产生可轮询的完成事件。 */
    wr.send_flags = IBV_SEND_SIGNALED;

    return ibv_post_send(endpoint->qp, &wr, &bad_wr);
}

static int poll_two(struct session *session, const char *round)
{
    struct ibv_wc completions[2];
    struct timespec start;
    struct timespec now;
    int total = 0;
    int count;
    int i;

    /* 每轮预期一个 SEND CQE 和一个 RECV CQE，并设置超时防止永久忙等。 */
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (total < 2) {
        count = ibv_poll_cq(session->cq, 2 - total, completions + total);
        if (count < 0)
            return -1;

        for (i = 0; i < count; ++i) {
            const struct ibv_wc *wc = &completions[total + i];

            printf("round=%s cqe_wr_id=%llu status=%s opcode=%d byte_len=%u\n",
                   round, (unsigned long long)wc->wr_id,
                   ibv_wc_status_str(wc->status), wc->opcode, wc->byte_len);
        }
        total += count;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec > 5)
            return -1;
    }

    /* poll 到 CQE 不等于操作成功，还必须检查每个 WC status。 */
    for (i = 0; i < 2; ++i) {
        if (completions[i].status != IBV_WC_SUCCESS)
            return -1;
    }
    return 0;
}

static int transfer(struct session *session, struct endpoint *source,
                    struct endpoint *destination, const char *text,
                    const char *round, uint64_t wr_id_base)
{
    int payload_matches;

    /*
     * RC SEND/RECV 的关键顺序：先给接收端 RQ 投递 WR，再给发送端 SQ 投递 WR。
     * 如果接收端没有可用 WR，发送端会遇到 RNR 并进入重试。
     */
    memset(destination->buf, 0, BUFFER_SIZE);
    if (post_recv(destination, wr_id_base + 2) != 0 ||
        post_send(source, text, wr_id_base + 1) != 0 ||
        poll_two(session, round) != 0)
        return -1;

    /* CQE 成功只证明 verbs 操作完成，业务层仍需核对实际 payload。 */
    payload_matches = strcmp(destination->buf, text) == 0;
    printf("round=%s receiver=%s payload=%s verify=%s\n",
           round, destination->name, destination->buf,
           payload_matches ? "pass" : "fail");
    return payload_matches ? 0 : -1;
}

static void cleanup(struct session *session)
{
    struct endpoint *endpoints[2] = {&session->right, &session->left};
    int i;

    /* QP 引用 CQ/PD，MR 引用 PD，因此按 QP/MR -> CQ -> PD -> context 销毁。 */
    for (i = 0; i < 2; ++i) {
        if (endpoints[i]->qp != NULL)
            ibv_destroy_qp(endpoints[i]->qp);
        if (endpoints[i]->mr != NULL)
            ibv_dereg_mr(endpoints[i]->mr);
        free(endpoints[i]->buf);
    }
    if (session->cq != NULL)
        ibv_destroy_cq(session->cq);
    if (session->pd != NULL)
        ibv_dealloc_pd(session->pd);
    if (session->ctx != NULL)
        ibv_close_device(session->ctx);
    if (session->list != NULL)
        ibv_free_device_list(session->list);
}

int main(int argc, char **argv)
{
    static const struct option options[] = {
        {"device", required_argument, NULL, 'd'},
        {"port", required_argument, NULL, 'p'},
        {"gid-index", required_argument, NULL, 'g'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    struct session session = {0};
    const char *device_name = "rxe0";
    int port = 1;
    int gid_index = 1;
    int option;
    int device_count = 0;
    int i;
    int rc = EXIT_FAILURE;

    /* 第一阶段：解析设备、端口和 GID index，不创建任何 verbs 资源。 */
    while ((option = getopt_long(argc, argv, "d:p:g:h", options, NULL)) != -1) {
        switch (option) {
        case 'd': device_name = optarg; break;
        case 'p': port = atoi(optarg); break;
        case 'g': gid_index = atoi(optarg); break;
        case 'h':
            puts("usage: rdma-rc-pingpong [--device NAME] [--port N] [--gid-index N]");
            return EXIT_SUCCESS;
        default:
            return EXIT_FAILURE;
        }
    }

    /* 第二阶段：枚举并打开指定 provider-visible RDMA device。 */
    session.list = ibv_get_device_list(&device_count);
    for (i = 0; i < device_count; ++i) {
        if (strcmp(device_name, ibv_get_device_name(session.list[i])) == 0) {
            session.ctx = ibv_open_device(session.list[i]);
            break;
        }
    }
    session.port = (uint8_t)port;
    session.gid_index = gid_index;

    /* RoCE 建链需要有效 port 属性和与 ens34 地址对应的 GID。 */
    if (session.ctx == NULL ||
        ibv_query_port(session.ctx, session.port, &session.port_attr) != 0 ||
        ibv_query_gid(session.ctx, session.port, gid_index, &session.gid) != 0)
        goto out;

    /* 第三阶段：创建共享 PD/CQ，再为左右端点创建独立 MR/QP。 */
    session.pd = ibv_alloc_pd(session.ctx);
    session.cq = session.pd != NULL
                     ? ibv_create_cq(session.ctx, CQ_DEPTH, NULL, NULL, 0)
                     : NULL;
    if (session.cq == NULL ||
        setup_endpoint(&session, &session.left, "left", 0x111111) != 0 ||
        setup_endpoint(&session, &session.right, "right", 0x222222) != 0)
        goto out;

    /* 第四阶段：双方互填 QPN/GID/PSN，完成 RC QP 状态迁移。 */
    if (to_init(&session, &session.left) != 0 ||
        to_init(&session, &session.right) != 0 ||
        to_rtr(&session, &session.left, &session.right) != 0 ||
        to_rtr(&session, &session.right, &session.left) != 0 ||
        to_rts(&session.left) != 0 || to_rts(&session.right) != 0)
        goto out;

    printf("connection=RTS left_qpn=%u right_qpn=%u\n",
           session.left.qp->qp_num, session.right.qp->qp_num);
    /* 第五阶段：left -> right 为 ping，right -> left 为 pong。 */
    if (transfer(&session, &session.left, &session.right,
                 "ping-from-left", "ping", 100) != 0 ||
        transfer(&session, &session.right, &session.left,
                 "pong-from-right", "pong", 200) != 0)
        goto out;

    puts("pingpong_result=pass");
    rc = EXIT_SUCCESS;

out:
    /* 任一步失败都进入同一个逆序清理出口，支持部分初始化状态。 */
    cleanup(&session);
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}
