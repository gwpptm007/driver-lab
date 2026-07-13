#define _POSIX_C_SOURCE 200112L
#include "rdma_cs.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void rdma_cs_gid_to_hex(const union ibv_gid *gid, char out[RDMA_CS_GID_HEX_SIZE])
{
    int i;

    /*
     * libibverbs 的 GID 是 16 字节二进制。控制面为了可读性使用 32 位
     * hex 字符串传输，接收方再转回 union ibv_gid。
     */
    for (i = 0; i < 16; ++i)
        snprintf(out + i * 2, 3, "%02x", gid->raw[i]);
    out[RDMA_CS_GID_HEX_SIZE - 1] = '\0';
}

static int hex_nibble(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

int rdma_cs_gid_from_hex(const char *hex, union ibv_gid *gid)
{
    int i;

    if (strlen(hex) != RDMA_CS_GID_HEX_SIZE - 1)
        return -1;
    memset(gid, 0, sizeof(*gid));
    for (i = 0; i < 16; ++i) {
        int high = hex_nibble(hex[i * 2]);
        int low = hex_nibble(hex[i * 2 + 1]);

        if (high < 0 || low < 0)
            return -1;
        gid->raw[i] = (uint8_t)((high << 4) | low);
    }
    return 0;
}

static int gid_is_zero(const union ibv_gid *gid)
{
    int i;

    /*
     * RXE/GID table 在网络状态变化后可能出现全 0 GID。
     * 全 0 GID 不是一个可用 RoCE 路径，继续建 QP 只会在 RTR 阶段失败。
     */
    for (i = 0; i < 16; ++i) {
        if (gid->raw[i] != 0)
            return 0;
    }
    return 1;
}

static struct ibv_qp *create_qp(struct rdma_cs_context *context)
{
    struct ibv_qp_init_attr attr = {0};

    /*
     * RC QP 内部有 SQ 和 RQ：
     * - SQ 放 SEND/WRITE/READ 这些发送侧 WR；
     * - RQ 放 RECV WR。
     *
     * 这里让 send_cq 和 recv_cq 指向同一个 CQ，简化学习模型。
     * CQE 里 opcode/wr_id 会告诉我们完成的是 SEND、RECV、WRITE 还是 READ。
     */
    attr.send_cq = context->cq;
    attr.recv_cq = context->cq;
    attr.qp_type = IBV_QPT_RC;
    attr.cap.max_send_wr = 16;
    attr.cap.max_recv_wr = 16;
    attr.cap.max_send_sge = 1;
    attr.cap.max_recv_sge = 1;

    return ibv_create_qp(context->pd, &attr);
}

int rdma_cs_resources_create(struct rdma_cs_context *context,
                             const struct rdma_cs_options *options,
                             uint32_t psn)
{
    int device_count = 0;
    int i;

    memset(context, 0, sizeof(*context));
    context->psn = psn;
    context->ib_port = (uint8_t)options->ib_port;
    context->gid_index = options->gid_index;

    /*
     * 第一层：枚举 provider 可见的 RDMA device，并打开指定设备。
     * 如果这里拿不到 rxe0，说明不是代码数据面问题，而是环境里没有
     * provider-visible RDMA device。
     */
    context->device_list = ibv_get_device_list(&device_count);
    if (context->device_list == NULL)
        return -1;

    for (i = 0; i < device_count; ++i) {
        if (strcmp(options->device_name,
                   ibv_get_device_name(context->device_list[i])) == 0) {
            context->ctx = ibv_open_device(context->device_list[i]);
            break;
        }
    }
    if (context->ctx == NULL)
        return -1;

    /*
     * RoCE 建链依赖 port attr 和 GID：
     * - port_attr 提供 active_mtu；
     * - gid 描述以太网上的 RoCE 地址。
     *
     * 本实验脚本会先把 fe80::34 加到 ens34，再重建 rxe0，
     * 因此 gid-index 1 对应 fe80::34。这个顺序很重要。
     */
    if (ibv_query_port(context->ctx, context->ib_port,
                       &context->port_attr) != 0 ||
        ibv_query_gid(context->ctx, context->ib_port, context->gid_index,
                      &context->gid) != 0)
        return -1;
    if (gid_is_zero(&context->gid))
        return -1;

    context->pd = ibv_alloc_pd(context->ctx);
    if (context->pd == NULL)
        return -1;

    if (posix_memalign((void **)&context->buf, 4096,
                       RDMA_CS_BUFFER_SIZE) != 0)
        return -1;
    memset(context->buf, 0, RDMA_CS_BUFFER_SIZE);

    /*
     * MR 注册的含义：
     * - pin 住这段用户态内存；
     * - 给本地 SGE 生成 lkey；
     * - 给对端 one-sided 操作生成 rkey。
     *
     * 本项目要同时验证 SEND/RECV、RDMA WRITE、RDMA READ，所以 MR 权限
     * 包含 LOCAL_WRITE、REMOTE_READ、REMOTE_WRITE。
     */
    context->mr = ibv_reg_mr(context->pd, context->buf, RDMA_CS_BUFFER_SIZE,
                             IBV_ACCESS_LOCAL_WRITE |
                             IBV_ACCESS_REMOTE_READ |
                             IBV_ACCESS_REMOTE_WRITE);
    if (context->mr == NULL)
        return -1;

    context->cq = ibv_create_cq(context->ctx, RDMA_CS_CQ_DEPTH, NULL, NULL, 0);
    if (context->cq == NULL)
        return -1;

    context->qp = create_qp(context);
    return context->qp != NULL ? 0 : -1;
}

int rdma_cs_metadata_from_context(struct rdma_cs_metadata *metadata,
                                  const struct rdma_cs_context *context,
                                  enum rdma_cs_role role)
{
    /*
     * 把本进程创建出来的真实 verbs 信息导出给 TCP 控制面：
     * - qpn/psn/gid 用于 QP RTR；
     * - addr/rkey 用于 RDMA READ/WRITE；
     * - role 用于日志区分 server/client。
     */
    memset(metadata, 0, sizeof(*metadata));
    snprintf(metadata->role, sizeof(metadata->role), "%s",
             role == RDMA_CS_ROLE_SERVER ? "server" : "client");
    metadata->qpn = context->qp->qp_num;
    metadata->psn = context->psn;
    metadata->gid_index = context->gid_index;
    rdma_cs_gid_to_hex(&context->gid, metadata->gid);
    metadata->addr = (uintptr_t)context->buf;
    metadata->rkey = context->mr->rkey;
    return 0;
}

void rdma_cs_resources_destroy(struct rdma_cs_context *context)
{
    /*
     * 资源必须按依赖关系反向销毁：
     * QP 引用 CQ/PD，MR 引用 PD，PD 引用 context。
     * 如果顺序反了，真实 provider 上可能出现销毁失败或资源泄漏。
     */
    if (context->qp != NULL)
        ibv_destroy_qp(context->qp);
    if (context->cq != NULL)
        ibv_destroy_cq(context->cq);
    if (context->mr != NULL)
        ibv_dereg_mr(context->mr);
    free(context->buf);
    if (context->pd != NULL)
        ibv_dealloc_pd(context->pd);
    if (context->ctx != NULL)
        ibv_close_device(context->ctx);
    if (context->device_list != NULL)
        ibv_free_device_list(context->device_list);
    memset(context, 0, sizeof(*context));
}

static int qp_to_init(struct rdma_cs_context *context)
{
    struct ibv_qp_attr attr = {0};

    /*
     * RESET -> INIT 是本地属性阶段：
     * - port_num 说明这个 QP 从哪个 RDMA port 出去；
     * - pkey_index 在 RoCE/RXE 中通常使用 0；
     * - qp_access_flags 决定对端能不能 READ/WRITE 本端 MR。
     */
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = context->ib_port;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                           IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;

    return ibv_modify_qp(context->qp, &attr,
                         IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                         IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
}

static int qp_to_rtr(struct rdma_cs_context *context,
                     const struct rdma_cs_metadata *remote)
{
    struct ibv_qp_attr attr = {0};
    union ibv_gid remote_gid;

    if (rdma_cs_gid_from_hex(remote->gid, &remote_gid) != 0)
        return -1;

    /*
     * INIT -> RTR 是接收方向建链：
     * - dest_qp_num 是对端 QPN；
     * - rq_psn 是“期望对端从哪个 PSN 开始发”；
     * - path_mtu 必须来自本地 port attr；
     * - ah_attr.grh.dgid 是对端 GID。
     *
     * RoCE 没有 InfiniBand subnet manager 分配的 LID 路由，这里必须设置
     * is_global=1，通过 GRH/GID 描述路径。
     */
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = context->port_attr.active_mtu;
    attr.dest_qp_num = remote->qpn;
    attr.rq_psn = remote->psn;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 12;
    attr.ah_attr.is_global = 1;
    attr.ah_attr.port_num = context->ib_port;
    attr.ah_attr.grh.dgid = remote_gid;
    attr.ah_attr.grh.sgid_index = context->gid_index;
    attr.ah_attr.grh.hop_limit = 1;

    return ibv_modify_qp(context->qp, &attr,
                         IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU |
                         IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
                         IBV_QP_MAX_DEST_RD_ATOMIC |
                         IBV_QP_MIN_RNR_TIMER);
}

static int qp_to_rts(struct rdma_cs_context *context)
{
    struct ibv_qp_attr attr = {0};

    /*
     * RTR -> RTS 是发送方向建链：
     * - sq_psn 是本端发送队列起始 PSN；
     * - retry_cnt/rnr_retry 决定 RC 的重试行为；
     * - max_rd_atomic 决定本端可同时发起多少个 RDMA READ。
     */
    attr.qp_state = IBV_QPS_RTS;
    attr.sq_psn = context->psn;
    attr.timeout = 14;
    attr.retry_cnt = 7;
    /*
     * rnr_retry=0 让 skip-recv 故障 case 快速暴露 RNR retry exceeded。
     * 正常路径中 server 会提前 post RECV，因此不会触发 RNR。
     */
    attr.rnr_retry = 0;
    attr.max_rd_atomic = 1;

    return ibv_modify_qp(context->qp, &attr,
                         IBV_QP_STATE | IBV_QP_TIMEOUT |
                         IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                         IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
}

int rdma_cs_qp_to_rts(struct rdma_cs_context *context,
                      const struct rdma_cs_metadata *remote)
{
    return qp_to_init(context) == 0 &&
           qp_to_rtr(context, remote) == 0 &&
           qp_to_rts(context) == 0 ? 0 : -1;
}

int rdma_cs_post_recv(struct rdma_cs_context *context, uint64_t wr_id)
{
    /*
     * RECV 必须先 post 到 RQ。否则 client SEND 到达时 server 没有可用
     * receive buffer，会触发 RNR retry，严重时导致超时。
     */
    struct ibv_sge sge = {
        .addr = (uintptr_t)context->buf,
        .length = RDMA_CS_BUFFER_SIZE,
        .lkey = context->mr->lkey,
    };
    struct ibv_recv_wr wr = {
        .wr_id = wr_id,
        .sg_list = &sge,
        .num_sge = 1,
    };
    struct ibv_recv_wr *bad_wr = NULL;

    return ibv_post_recv(context->qp, &wr, &bad_wr);
}

int rdma_cs_post_send_flags(struct rdma_cs_context *context, const char *payload,
                            uint64_t wr_id, int send_flags)
{
    struct ibv_sge sge;
    struct ibv_send_wr wr = {0};
    struct ibv_send_wr *bad_wr = NULL;

    /*
     * SEND 使用本地 SGE 指向要发送的 payload。对端接收到哪里，
     * 不是由 SEND 指定，而是由对端提前 post 的 RECV WR 指定。
     */
    snprintf(context->buf, RDMA_CS_BUFFER_SIZE, "%s", payload);
    sge.addr = (uintptr_t)context->buf;
    sge.length = (uint32_t)strlen(context->buf) + 1;
    sge.lkey = context->mr->lkey;

    wr.wr_id = wr_id;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = send_flags;

    return ibv_post_send(context->qp, &wr, &bad_wr);
}

int rdma_cs_post_send(struct rdma_cs_context *context, const char *payload,
                      uint64_t wr_id)
{
    return rdma_cs_post_send_flags(context, payload, wr_id,
                                   IBV_SEND_SIGNALED);
}

static int post_one_sided(struct rdma_cs_context *context,
                          const struct rdma_cs_metadata *remote,
                          enum ibv_wr_opcode opcode, size_t length,
                          uint64_t wr_id, int wrong_rkey, int wrong_addr)
{
    /*
     * one-sided WR 与 SEND 最大的区别：
     * - 本地仍然使用 SGE/lkey 描述本地内存；
     * - 远端使用 remote_addr/rkey 描述目标 MR；
     * - 远端不需要 post RECV，也不会产生远端 CQE。
     */
    struct ibv_sge sge = {
        .addr = (uintptr_t)context->buf,
        .length = (uint32_t)length,
        .lkey = context->mr->lkey,
    };
    struct ibv_send_wr wr = {0};
    struct ibv_send_wr *bad_wr = NULL;

    wr.wr_id = wr_id;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = opcode;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.rdma.remote_addr = wrong_addr ?
                                 (remote->addr + RDMA_CS_BUFFER_SIZE * 4ULL) :
                                 remote->addr;
    /*
     * wrong_rkey 模式故意翻转一位 rkey。正确行为不是“成功写入”，
     * 而是 CQE 返回 remote access error，用它验证安全边界。
     */
    wr.wr.rdma.rkey = wrong_rkey ? (remote->rkey ^ 1U) : remote->rkey;

    return ibv_post_send(context->qp, &wr, &bad_wr);
}

int rdma_cs_post_write(struct rdma_cs_context *context,
                       const struct rdma_cs_metadata *remote,
                       const char *payload, uint64_t wr_id,
                       int wrong_rkey, int wrong_addr)
{
    snprintf(context->buf, RDMA_CS_BUFFER_SIZE, "%s", payload);
    return post_one_sided(context, remote, IBV_WR_RDMA_WRITE,
                          strlen(payload) + 1, wr_id, wrong_rkey, wrong_addr);
}

int rdma_cs_post_read(struct rdma_cs_context *context,
                      const struct rdma_cs_metadata *remote,
                      size_t length, uint64_t wr_id)
{
    memset(context->buf, 0, RDMA_CS_BUFFER_SIZE);
    return post_one_sided(context, remote, IBV_WR_RDMA_READ,
                          length, wr_id, 0, 0);
}

static int poll_one(struct rdma_cs_context *context, const char *tag,
                    int expect_success)
{
    struct ibv_wc wc;
    struct timespec start;
    struct timespec now;
    int count;

    /*
     * CQ polling 是 RDMA 用户态数据面的核心循环。这里用 busy poll 加
     * 超时，既贴近真实 fast path，又避免测试卡死。
     */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        count = ibv_poll_cq(context->cq, 1, &wc);
        if (count < 0)
            return -1;
        if (count == 1)
            break;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if ((now.tv_sec - start.tv_sec) * 1000 >
            RDMA_CS_CQ_TIMEOUT_MS)
            return -1;
    }

    printf("%s cqe_wr_id=%llu status=%s opcode=%d byte_len=%u\n",
           tag, (unsigned long long)wc.wr_id,
           ibv_wc_status_str(wc.status), wc.opcode, wc.byte_len);
    if (expect_success)
        return wc.status == IBV_WC_SUCCESS ? 0 : -1;
    return wc.status != IBV_WC_SUCCESS ? 0 : -1;
}

int rdma_cs_poll_success(struct rdma_cs_context *context, const char *tag)
{
    return poll_one(context, tag, 1);
}

int rdma_cs_poll_expect_error(struct rdma_cs_context *context, const char *tag)
{
    return poll_one(context, tag, 0);
}
