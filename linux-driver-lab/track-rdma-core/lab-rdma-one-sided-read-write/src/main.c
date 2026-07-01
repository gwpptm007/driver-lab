#define _POSIX_C_SOURCE 200112L
#include "one_sided.h"
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
    /* 每个端点使用独立 buffer/MR，分别模拟本地和远端内存。 */
    if (posix_memalign((void **)&endpoint->buf, 4096, BUFFER_SIZE) != 0)
        return -1;

    memset(endpoint->buf, 0, BUFFER_SIZE);
    /*
     * LOCAL_WRITE 允许本地设备写入；REMOTE_READ/WRITE 把 rkey 权限暴露给对端。
     * one-sided 操作还必须携带目标虚拟地址，rkey 单独不足以定位内存。
     */
    endpoint->mr = ibv_reg_mr(session->pd, endpoint->buf, BUFFER_SIZE,
                              IBV_ACCESS_LOCAL_WRITE |
                              IBV_ACCESS_REMOTE_READ |
                              IBV_ACCESS_REMOTE_WRITE);
    endpoint->qp = create_qp(session);

    return endpoint->mr != NULL && endpoint->qp != NULL ? 0 : -1;
}

static int poll_one(struct session *session, const char *operation)
{
    struct ibv_wc completion;
    struct timespec start;
    struct timespec now;
    int count;

    /* one-sided 操作只在发起端产生完成事件，远端没有 Receive CQE。 */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        count = ibv_poll_cq(session->cq, 1, &completion);
        if (count < 0)
            return -1;
        if (count == 1)
            break;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec > 5)
            return -1;
    }

    printf("operation=%s cqe_wr_id=%llu status=%s opcode=%d\n",
           operation, (unsigned long long)completion.wr_id,
           ibv_wc_status_str(completion.status), completion.opcode);
    return completion.status == IBV_WC_SUCCESS ? 0 : -1;
}

static int post_one_sided(struct session *session, struct endpoint *local,
                          struct endpoint *remote, enum ibv_wr_opcode opcode,
                          size_t length, uint64_t wr_id, const char *operation)
{
    struct ibv_sge sge = {
        .addr = (uintptr_t)local->buf,
        .length = (uint32_t)length,
        .lkey = local->mr->lkey,
    };
    struct ibv_send_wr wr = {0};
    struct ibv_send_wr *bad_wr = NULL;

    wr.wr_id = wr_id;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = opcode;
    wr.send_flags = IBV_SEND_SIGNALED;
    /* address 与 rkey 共同描述远端 MR；远端应用不需要 post receive。 */
    wr.wr.rdma.remote_addr = (uintptr_t)remote->buf;
    wr.wr.rdma.rkey = remote->mr->rkey;

    if (ibv_post_send(local->qp, &wr, &bad_wr) != 0)
        return -1;
    return poll_one(session, operation);
}

static int run_write(struct session *session)
{
    const char *payload = "written-by-left-with-rdma-write";

    memset(session->right.buf, 0, BUFFER_SIZE);
    snprintf(session->left.buf, BUFFER_SIZE, "%s", payload);
    if (post_one_sided(session, &session->left, &session->right,
                       IBV_WR_RDMA_WRITE, strlen(payload) + 1, 301,
                       "RDMA_WRITE") != 0)
        return -1;

    printf("operation=RDMA_WRITE remote_payload=%s verify=%s\n",
           session->right.buf,
           strcmp(session->right.buf, payload) == 0 ? "pass" : "fail");
    return strcmp(session->right.buf, payload) == 0 ? 0 : -1;
}

static int run_read(struct session *session)
{
    const char *payload = "read-from-right-with-rdma-read";

    snprintf(session->right.buf, BUFFER_SIZE, "%s", payload);
    memset(session->left.buf, 0, BUFFER_SIZE);
    if (post_one_sided(session, &session->left, &session->right,
                       IBV_WR_RDMA_READ, strlen(payload) + 1, 401,
                       "RDMA_READ") != 0)
        return -1;

    printf("operation=RDMA_READ local_payload=%s verify=%s\n",
           session->left.buf,
           strcmp(session->left.buf, payload) == 0 ? "pass" : "fail");
    return strcmp(session->left.buf, payload) == 0 ? 0 : -1;
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
            puts("usage: rdma-one-sided [--device NAME] [--port N] [--gid-index N]");
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
    /* 第五阶段：WRITE 修改远端 MR，READ 从远端 MR 拉取数据。 */
    printf("remote_metadata address=%p rkey=0x%x length=%u\n",
           (void *)session.right.buf, session.right.mr->rkey, BUFFER_SIZE);
    if (run_write(&session) != 0 || run_read(&session) != 0)
        goto out;

    puts("one_sided_result=pass");
    rc = EXIT_SUCCESS;

out:
    /* 任一步失败都进入同一个逆序清理出口，支持部分初始化状态。 */
    cleanup(&session);
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}
