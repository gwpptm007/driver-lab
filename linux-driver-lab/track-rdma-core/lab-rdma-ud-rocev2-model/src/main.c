#define _POSIX_C_SOURCE 200112L
#include "ud_lab.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static struct ibv_qp *create_ud_qp(struct ud_session *session)
{
    struct ibv_qp_init_attr attr = {0};
    attr.send_cq = session->cq;
    attr.recv_cq = session->cq;
    attr.qp_type = IBV_QPT_UD;
    attr.cap.max_send_wr = 16;
    attr.cap.max_recv_wr = 16;
    attr.cap.max_send_sge = 1;
    attr.cap.max_recv_sge = 1;
    return ibv_create_qp(session->pd, &attr);
}

static int setup_endpoint(struct ud_session *session,
                          struct ud_endpoint *endpoint,
                          const char *name, uint32_t psn)
{
    endpoint->name = name;
    endpoint->psn = psn;
    if (posix_memalign((void **)&endpoint->buffer, 4096,
                       UD_BUFFER_SIZE) != 0)
        return -1;
    memset(endpoint->buffer, 0, UD_BUFFER_SIZE);
    endpoint->mr = ibv_reg_mr(session->pd, endpoint->buffer,
                              UD_BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE);
    endpoint->qp = create_ud_qp(session);
    return endpoint->mr != NULL && endpoint->qp != NULL ? 0 : -1;
}

static int move_ud_to_rts(struct ud_session *session,
                          struct ud_endpoint *endpoint)
{
    struct ibv_qp_attr attr = {0};

    /* UD 的 INIT 需要本地 port、P_Key 和 Q_Key，不配置 RC access flags。 */
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = session->port;
    attr.pkey_index = 0;
    attr.qkey = UD_QKEY;
    if (ibv_modify_qp(endpoint->qp, &attr,
                      IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                      IBV_QP_PORT | IBV_QP_QKEY) != 0)
        return -1;

    /* UD 没有面向连接的 peer 参数，RTR 只修改状态。 */
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTR;
    if (ibv_modify_qp(endpoint->qp, &attr, IBV_QP_STATE) != 0)
        return -1;

    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTS;
    attr.sq_psn = endpoint->psn;
    return ibv_modify_qp(endpoint->qp, &attr,
                         IBV_QP_STATE | IBV_QP_SQ_PSN);
}

static int post_receive(struct ud_endpoint *receiver)
{
    struct ibv_sge sge = {
        .addr = (uintptr_t)receiver->buffer,
        .length = UD_BUFFER_SIZE,
        .lkey = receiver->mr->lkey,
    };
    struct ibv_recv_wr wr = {
        .wr_id = 702,
        .sg_list = &sge,
        .num_sge = 1,
    };
    struct ibv_recv_wr *bad_wr = NULL;

    /* UD 接收数据前会保留 40 字节 GRH，因此 SGE 必须为其预留空间。 */
    return ibv_post_recv(receiver->qp, &wr, &bad_wr);
}

static int post_send(struct ud_session *session, const char *payload)
{
    struct ibv_sge sge;
    struct ibv_send_wr wr = {0};
    struct ibv_send_wr *bad_wr = NULL;

    snprintf((char *)session->sender.buffer, UD_BUFFER_SIZE, "%s", payload);
    sge.addr = (uintptr_t)session->sender.buffer;
    sge.length = (uint32_t)strlen(payload) + 1;
    sge.lkey = session->sender.mr->lkey;
    wr.wr_id = 701;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = IBV_SEND_SIGNALED;

    /* UD 的每个 SEND WR 都必须携带 AH、目标 QPN 和目标 Q_Key。 */
    wr.wr.ud.ah = session->address_handle;
    wr.wr.ud.remote_qpn = session->receiver.qp->qp_num;
    wr.wr.ud.remote_qkey = UD_QKEY;
    return ibv_post_send(session->sender.qp, &wr, &bad_wr);
}

static int poll_completions(struct ud_session *session)
{
    struct ibv_wc completions[2];
    struct timespec start, now;
    int total = 0;
    int count;
    int i;

    clock_gettime(CLOCK_MONOTONIC, &start);
    while (total < 2) {
        count = ibv_poll_cq(session->cq, 2 - total,
                            completions + total);
        if (count < 0)
            return -1;
        total += count;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec > 5)
            return -1;
    }
    for (i = 0; i < 2; ++i) {
        printf("cqe_wr_id=%llu status=%s opcode=%d byte_len=%u\n",
               (unsigned long long)completions[i].wr_id,
               ibv_wc_status_str(completions[i].status),
               completions[i].opcode, completions[i].byte_len);
        if (completions[i].status != IBV_WC_SUCCESS)
            return -1;
    }
    return 0;
}

static void cleanup(struct ud_session *session)
{
    struct ud_endpoint *endpoints[2] = {
        &session->receiver, &session->sender,
    };
    int i;

    if (session->address_handle != NULL)
        ibv_destroy_ah(session->address_handle);
    for (i = 0; i < 2; ++i) {
        if (endpoints[i]->qp != NULL)
            ibv_destroy_qp(endpoints[i]->qp);
        if (endpoints[i]->mr != NULL)
            ibv_dereg_mr(endpoints[i]->mr);
        free(endpoints[i]->buffer);
    }
    if (session->cq != NULL)
        ibv_destroy_cq(session->cq);
    if (session->pd != NULL)
        ibv_dealloc_pd(session->pd);
    if (session->context != NULL)
        ibv_close_device(session->context);
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
    struct ud_session session = {0};
    struct ibv_ah_attr ah_attr = {0};
    const char *device_name = "rxe0";
    const char *payload = "ud-datagram-over-rocev2";
    int port = 1, gid_index = 1, option, count = 0, i;
    int rc = EXIT_FAILURE;

    while ((option = getopt_long(argc, argv, "d:p:g:h",
                                 options, NULL)) != -1) {
        if (option == 'd') device_name = optarg;
        else if (option == 'p') port = atoi(optarg);
        else if (option == 'g') gid_index = atoi(optarg);
        else {
            puts("usage: rdma-ud-demo [--device NAME] [--port N] [--gid-index N]");
            return option == 'h' ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }

    session.list = ibv_get_device_list(&count);
    for (i = 0; i < count; ++i) {
        if (strcmp(device_name, ibv_get_device_name(session.list[i])) == 0) {
            session.context = ibv_open_device(session.list[i]);
            break;
        }
    }
    session.port = (uint8_t)port;
    session.gid_index = gid_index;
    if (session.context == NULL ||
        ibv_query_gid(session.context, session.port, gid_index,
                      &session.gid) != 0)
        goto out;

    session.pd = ibv_alloc_pd(session.context);
    session.cq = session.pd != NULL
                     ? ibv_create_cq(session.context, 32, NULL, NULL, 0)
                     : NULL;
    if (session.cq == NULL ||
        setup_endpoint(&session, &session.sender, "sender", 0x123456) != 0 ||
        setup_endpoint(&session, &session.receiver, "receiver", 0x654321) != 0 ||
        move_ud_to_rts(&session, &session.sender) != 0 ||
        move_ud_to_rts(&session, &session.receiver) != 0)
        goto out;

    ah_attr.is_global = 1;
    ah_attr.port_num = session.port;
    ah_attr.grh.dgid = session.gid;
    ah_attr.grh.sgid_index = session.gid_index;
    ah_attr.grh.hop_limit = 1;
    session.address_handle = ibv_create_ah(session.pd, &ah_attr);
    if (session.address_handle == NULL ||
        post_receive(&session.receiver) != 0 ||
        post_send(&session, payload) != 0 ||
        poll_completions(&session) != 0)
        goto out;

    printf("transport=UD qkey=0x%x sender_qpn=%u receiver_qpn=%u\n",
           UD_QKEY, session.sender.qp->qp_num,
           session.receiver.qp->qp_num);
    printf("grh_bytes=%u payload=%s verify=%s\n",
           UD_GRH_SIZE, session.receiver.buffer + UD_GRH_SIZE,
           strcmp((char *)session.receiver.buffer + UD_GRH_SIZE,
                  payload) == 0 ? "pass" : "fail");
    if (strcmp((char *)session.receiver.buffer + UD_GRH_SIZE,
               payload) != 0)
        goto out;

    puts("ud_result=pass");
    rc = EXIT_SUCCESS;
out:
    cleanup(&session);
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}
