#include "qp_lab.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

const char *qp_state_name(enum ibv_qp_state state)
{
    switch (state) {
    case IBV_QPS_RESET: return "RESET";
    case IBV_QPS_INIT: return "INIT";
    case IBV_QPS_RTR: return "RTR";
    case IBV_QPS_RTS: return "RTS";
    case IBV_QPS_ERR: return "ERR";
    default: return "OTHER";
    }
}

static int print_state(const char *name, struct ibv_qp *qp)
{
    struct ibv_qp_attr attr = {0};
    struct ibv_qp_init_attr init = {0};
    if (ibv_query_qp(qp, &attr, IBV_QP_STATE, &init) != 0)
        return -1;
    printf("endpoint=%s qp_num=%u state=%s\n", name, qp->qp_num,
           qp_state_name(attr.qp_state));
    return 0;
}

static struct ibv_qp *create_rc_qp(struct qp_lab *lab)
{
    struct ibv_qp_init_attr attr = {0};
    attr.send_cq = lab->cq;
    attr.recv_cq = lab->cq;
    attr.qp_type = IBV_QPT_RC;
    attr.cap.max_send_wr = QP_QUEUE_DEPTH;
    attr.cap.max_recv_wr = QP_QUEUE_DEPTH;
    attr.cap.max_send_sge = 1;
    attr.cap.max_recv_sge = 1;
    return ibv_create_qp(lab->pd, &attr);
}

int qp_create_pair(struct qp_lab *lab)
{
    lab->left.psn = 0x111111;
    lab->right.psn = 0x222222;
    lab->left.qp = create_rc_qp(lab);
    lab->right.qp = create_rc_qp(lab);
    if (lab->left.qp == NULL || lab->right.qp == NULL)
        return -1;
    return print_state("left", lab->left.qp) || print_state("right", lab->right.qp);
}

static int modify_to_init(struct qp_lab *lab, struct qp_endpoint *endpoint)
{
    struct ibv_qp_attr attr = {0};
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = lab->port;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                           IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
    /* INIT 确定本地端口、P_Key 和该 QP 允许的远端访问类型。 */
    return ibv_modify_qp(endpoint->qp, &attr,
                         IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                         IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
}

static int modify_to_rtr(struct qp_lab *lab, struct qp_endpoint *local,
                         struct qp_endpoint *peer)
{
    struct ibv_qp_attr attr = {0};
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = lab->port_attr.active_mtu;
    attr.dest_qp_num = peer->qp->qp_num;
    attr.rq_psn = peer->psn;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 12;

    /* RoCE 没有有效 LID，Address Vector 通过 GRH 携带 GID。 */
    attr.ah_attr.is_global = 1;
    attr.ah_attr.port_num = lab->port;
    attr.ah_attr.grh.dgid = lab->gid;
    attr.ah_attr.grh.sgid_index = lab->gid_index;
    attr.ah_attr.grh.hop_limit = 1;

    return ibv_modify_qp(local->qp, &attr,
                         IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU |
                         IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
                         IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER);
}

static int modify_to_rts(struct qp_endpoint *endpoint)
{
    struct ibv_qp_attr attr = {0};
    attr.qp_state = IBV_QPS_RTS;
    attr.sq_psn = endpoint->psn;
    attr.timeout = 14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.max_rd_atomic = 1;
    /* RTS 配置本地发送 PSN、超时和重试策略。 */
    return ibv_modify_qp(endpoint->qp, &attr,
                         IBV_QP_STATE | IBV_QP_TIMEOUT |
                         IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                         IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
}

int qp_run_state_machine(struct qp_lab *lab)
{
    int rc;

    if (modify_to_init(lab, &lab->left) || modify_to_init(lab, &lab->right))
        return -1;
    print_state("left", lab->left.qp);
    print_state("right", lab->right.qp);

    errno = 0;
    rc = modify_to_rtr(lab, &lab->left, &lab->right);
    if (rc != 0) {
        printf("transition=left_INIT_to_RTR rc=%d errno=%d message=%s\n",
               rc, errno, strerror(errno));
        return -1;
    }
    errno = 0;
    rc = modify_to_rtr(lab, &lab->right, &lab->left);
    if (rc != 0) {
        printf("transition=right_INIT_to_RTR rc=%d errno=%d message=%s\n",
               rc, errno, strerror(errno));
        return -1;
    }
    print_state("left", lab->left.qp);
    print_state("right", lab->right.qp);

    if (modify_to_rts(&lab->left) || modify_to_rts(&lab->right))
        return -1;
    print_state("left", lab->left.qp);
    print_state("right", lab->right.qp);
    return 0;
}

int qp_run_invalid_transition(struct qp_lab *lab)
{
    struct ibv_qp *qp = create_rc_qp(lab);
    struct ibv_qp_attr attr = {0};
    int rc;
    int saved_errno;

    if (qp == NULL)
        return -1;
    attr.qp_state = IBV_QPS_RTR;
    errno = 0;
    rc = ibv_modify_qp(qp, &attr, IBV_QP_STATE);
    saved_errno = errno;
    printf("negative=RESET_to_RTR expected=failure actual=%s errno=%d result=%s\n",
           rc != 0 ? "failure" : "success", saved_errno,
           rc != 0 ? "pass" : "fail");
    ibv_destroy_qp(qp);
    return rc != 0 ? 0 : -1;
}
