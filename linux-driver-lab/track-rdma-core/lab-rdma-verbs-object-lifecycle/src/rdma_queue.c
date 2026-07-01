#include "rdma_resources.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

const char *rdma_qp_state_name(enum ibv_qp_state state)
{
    /* 将 verbs 枚举转换成稳定文本，既方便阅读，也方便测试脚本断言。 */
    switch (state) {
    case IBV_QPS_RESET:
        return "RESET";
    case IBV_QPS_INIT:
        return "INIT";
    case IBV_QPS_RTR:
        return "RTR";
    case IBV_QPS_RTS:
        return "RTS";
    case IBV_QPS_SQD:
        return "SQD";
    case IBV_QPS_SQE:
        return "SQE";
    case IBV_QPS_ERR:
        return "ERR";
    case IBV_QPS_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

int rdma_queue_create(struct rdma_resources *res)
{
    struct ibv_qp_init_attr init_attr;
    struct ibv_qp_attr qp_attr;
    struct ibv_qp_init_attr queried_init_attr;

    /*
     * CQ 存放完成事件 CQE，不存放待执行的 WR。
     * NULL channel 表示本实验采用主动轮询模型，不使用完成事件通知。
     */
    res->cq = ibv_create_cq(res->context, RDMA_CQ_DEPTH, NULL, NULL, 0);
    if (res->cq == NULL) {
        fprintf(stderr, "error=create_cq errno=%d message=%s\n",
                errno, strerror(errno));
        return -errno;
    }

    /*
     * QP 由 Send Queue 和 Receive Queue 组成。
     * 为了聚焦对象生命周期，SQ/RQ 共用一个 CQ，且每个 WR 只允许一个 SGE。
     */
    memset(&init_attr, 0, sizeof(init_attr));
    init_attr.send_cq = res->cq;
    init_attr.recv_cq = res->cq;
    init_attr.qp_type = IBV_QPT_RC;
    init_attr.cap.max_send_wr = RDMA_QUEUE_DEPTH;
    init_attr.cap.max_recv_wr = RDMA_QUEUE_DEPTH;
    init_attr.cap.max_send_sge = RDMA_MAX_SGE;
    init_attr.cap.max_recv_sge = RDMA_MAX_SGE;

    /* RC 表示 Reliable Connection，后续可支持 Send/Recv 和 RDMA READ/WRITE。 */
    res->qp = ibv_create_qp(res->pd, &init_attr);
    if (res->qp == NULL) {
        fprintf(stderr, "error=create_qp errno=%d message=%s\n",
                errno, strerror(errno));
        return -errno;
    }

    /*
     * 新创建的 QP 尚未配置 port、P_Key、对端 QPN/GID 和 PSN，
     * 因而必须处于 RESET。下一实验才负责 INIT -> RTR -> RTS。
     */
    memset(&qp_attr, 0, sizeof(qp_attr));
    memset(&queried_init_attr, 0, sizeof(queried_init_attr));
    if (ibv_query_qp(res->qp, &qp_attr, IBV_QP_STATE,
                     &queried_init_attr) != 0) {
        fprintf(stderr, "error=query_qp errno=%d message=%s\n",
                errno, strerror(errno));
        return -errno;
    }
    res->qp_state = qp_attr.qp_state;
    return 0;
}

void rdma_queue_destroy(struct rdma_resources *res)
{
    /* QP 引用了 CQ 和 PD，因此队列层必须先销毁 QP，再销毁 CQ。 */
    if (res->qp != NULL) {
        if (ibv_destroy_qp(res->qp) != 0) {
            fprintf(stderr, "warning=destroy_qp_failed errno=%d message=%s\n",
                    errno, strerror(errno));
        }
        res->qp = NULL;
    }
    if (res->cq != NULL) {
        if (ibv_destroy_cq(res->cq) != 0) {
            fprintf(stderr, "warning=destroy_cq_failed errno=%d message=%s\n",
                    errno, strerror(errno));
        }
        res->cq = NULL;
    }
}

void rdma_resources_cleanup(struct rdma_resources *res)
{
    /*
     * 依赖关系的严格逆序：
     * QP/CQ -> MR/buffer/PD -> context/device list。
     */
    rdma_queue_destroy(res);
    rdma_memory_destroy(res);
    rdma_device_close(res);
}
