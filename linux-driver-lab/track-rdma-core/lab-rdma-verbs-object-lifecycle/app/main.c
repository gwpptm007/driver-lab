#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <infiniband/verbs.h>

/*
 * 这个程序故意写得很直线，不做复杂封装。
 *
 * 学习目标只有一个：
 *   device -> context -> PD -> MR -> CQ -> QP -> 反向销毁
 *
 * 本 lab 暂时不 post send/recv work request。
 * 收发数据是后续 RC ping-pong lab 的内容。
 */
#define TEST_BUFFER_SIZE 4096
#define CQ_DEPTH 16
#define SQ_DEPTH 16
#define RQ_DEPTH 16
#define MAX_SGE 1

static void print_device_summary(struct ibv_device *dev, int index)
{
    /*
     * device 是 libibverbs 能看到的 RDMA 设备。
     * 在当前测试机上，启用 Soft-RoCE 后应该看到 rxe0。
     */
    printf("STEP 01 DEVICE[%d] name=%s dev_name=%s dev_path=%s ibdev_path=%s\n",
           index,
           ibv_get_device_name(dev),
           dev->dev_name ? dev->dev_name : "(null)",
           dev->dev_path ? dev->dev_path : "(null)",
           dev->ibdev_path ? dev->ibdev_path : "(null)");
}

static int inspect_device(struct ibv_device *dev)
{
    struct ibv_context *ctx = NULL;
    struct ibv_pd *pd = NULL;
    struct ibv_mr *mr = NULL;
    struct ibv_cq *cq = NULL;
    struct ibv_qp *qp = NULL;
    struct ibv_device_attr dev_attr;
    struct ibv_port_attr port_attr;
    struct ibv_qp_init_attr qp_init;
    void *buf = NULL;
    int rc = 1;

    memset(&dev_attr, 0, sizeof(dev_attr));
    memset(&port_attr, 0, sizeof(port_attr));
    memset(&qp_init, 0, sizeof(qp_init));

    /*
     * STEP 02: 打开 device。
     *
     * context 是用户态访问某个 RDMA device 的句柄。
     * 后面的 PD、CQ 等对象都依赖这个 context。
     */
    ctx = ibv_open_device(dev);
    if (!ctx) {
        printf("STEP 02 OPEN_DEVICE_FAIL errno=%d message=%s\n", errno, strerror(errno));
        goto out;
    }
    printf("STEP 02 OPEN_DEVICE_OK name=%s\n", ibv_get_device_name(dev));

    /*
     * STEP 03: 查询 device 能力。
     *
     * 这里打印的是 provider 暴露出来的能力上限：
     * 最大 QP 数、最大 CQ 数、最大 MR 数、每个 QP 支持多少 WR/SGE 等。
     */
    if (ibv_query_device(ctx, &dev_attr) != 0) {
        printf("STEP 03 QUERY_DEVICE_FAIL errno=%d message=%s\n", errno, strerror(errno));
        goto out;
    }
    printf("STEP 03 QUERY_DEVICE_OK max_qp=%d max_cq=%d max_mr=%d max_qp_wr=%d max_sge=%d phys_port_cnt=%u\n",
           dev_attr.max_qp,
           dev_attr.max_cq,
           dev_attr.max_mr,
           dev_attr.max_qp_wr,
           dev_attr.max_sge,
           dev_attr.phys_port_cnt);

    /*
     * STEP 04: 查询 port 1。
     *
     * 对 rxe0 来说，这是一个基于以太网网卡模拟出来的 RoCE-like port。
     * 后续 QP 状态迁移会用到 port 状态、MTU、GID 等信息。
     */
    if (dev_attr.phys_port_cnt > 0 && ibv_query_port(ctx, 1, &port_attr) == 0) {
        printf("STEP 04 QUERY_PORT_OK port=1 state=%u max_mtu=%u active_mtu=%u lid=%u gid_tbl_len=%d\n",
               port_attr.state,
               port_attr.max_mtu,
               port_attr.active_mtu,
               port_attr.lid,
               port_attr.gid_tbl_len);
    } else {
        printf("STEP 04 QUERY_PORT_SKIP_OR_FAIL port_count=%u errno=%d message=%s\n",
               dev_attr.phys_port_cnt, errno, strerror(errno));
    }

    /*
     * STEP 05: 分配 Protection Domain。
     *
     * PD 是资源隔离边界。MR 和 QP 都挂在 PD 下，
     * 这样 QP 不能随便访问不属于同一个 PD 的内存。
     */
    pd = ibv_alloc_pd(ctx);
    if (!pd) {
        printf("STEP 05 ALLOC_PD_FAIL errno=%d message=%s\n", errno, strerror(errno));
        goto out;
    }
    printf("STEP 05 ALLOC_PD_OK\n");

    /*
     * STEP 06: 分配用户态 buffer。
     *
     * RDMA 不能直接拿任意用户态地址做 DMA。
     * 这里先准备一段 buffer，下一步把它注册成 MR。
     */
    if (posix_memalign(&buf, 4096, TEST_BUFFER_SIZE) != 0) {
        printf("STEP 06 ALLOC_BUFFER_FAIL\n");
        goto out;
    }
    memset(buf, 0xab, TEST_BUFFER_SIZE);
    printf("STEP 06 ALLOC_BUFFER_OK addr=%p length=%d\n", buf, TEST_BUFFER_SIZE);

    /*
     * STEP 07: 注册 Memory Region。
     *
     * lkey：本地 SGE 访问这段内存时使用。
     * rkey：后续可以给远端，用于 RDMA READ/WRITE 授权。
     */
    mr = ibv_reg_mr(pd, buf, TEST_BUFFER_SIZE,
                    IBV_ACCESS_LOCAL_WRITE |
                    IBV_ACCESS_REMOTE_READ |
                    IBV_ACCESS_REMOTE_WRITE);
    if (!mr) {
        printf("STEP 07 REG_MR_FAIL errno=%d message=%s\n", errno, strerror(errno));
        goto out;
    }
    printf("STEP 07 REG_MR_OK addr=%p length=%zu lkey=0x%x rkey=0x%x\n",
           mr->addr, mr->length, mr->lkey, mr->rkey);

    /*
     * STEP 08: 创建 Completion Queue。
     *
     * 后续 send/recv WR 完成后，会从 CQ 里 poll 出 CQE。
     * 本 lab 先只证明 CQ 可以创建。
     */
    cq = ibv_create_cq(ctx, CQ_DEPTH, NULL, NULL, 0);
    if (!cq) {
        printf("STEP 08 CREATE_CQ_FAIL errno=%d message=%s\n", errno, strerror(errno));
        goto out;
    }
    printf("STEP 08 CREATE_CQ_OK depth=%d\n", CQ_DEPTH);

    /*
     * STEP 09: 创建 RC Queue Pair。
     *
     * QP 包含 Send Queue 和 Receive Queue。
     * 刚创建出来的 QP 还不能收发，后续 lab 会把它从 RESET
     * 切到 INIT/RTR/RTS。
     */
    qp_init.send_cq = cq;
    qp_init.recv_cq = cq;
    qp_init.qp_type = IBV_QPT_RC;
    qp_init.cap.max_send_wr = SQ_DEPTH;
    qp_init.cap.max_recv_wr = RQ_DEPTH;
    qp_init.cap.max_send_sge = MAX_SGE;
    qp_init.cap.max_recv_sge = MAX_SGE;

    qp = ibv_create_qp(pd, &qp_init);
    if (!qp) {
        printf("STEP 09 CREATE_QP_FAIL errno=%d message=%s\n", errno, strerror(errno));
        goto out;
    }
    printf("STEP 09 CREATE_QP_OK qp_num=%u qp_type=RC max_send_wr=%u max_recv_wr=%u max_send_sge=%u max_recv_sge=%u\n",
           qp->qp_num,
           qp_init.cap.max_send_wr,
           qp_init.cap.max_recv_wr,
           qp_init.cap.max_send_sge,
           qp_init.cap.max_recv_sge);

    printf("OBJECT_LIFECYCLE_PASS\n");
    rc = 0;

out:
    /*
     * STEP 10: 按反向顺序销毁。
     *
     * QP 依赖 CQ/PD，MR 依赖 PD，context 持有 device 资源。
     * 所以销毁顺序要和创建顺序反过来，避免资源关系混乱。
     */
    if (qp) {
        printf("STEP 10 DESTROY_QP_%s\n", ibv_destroy_qp(qp) == 0 ? "OK" : "FAIL");
    }
    if (cq) {
        printf("STEP 10 DESTROY_CQ_%s\n", ibv_destroy_cq(cq) == 0 ? "OK" : "FAIL");
    }
    if (mr) {
        printf("STEP 10 DEREG_MR_%s\n", ibv_dereg_mr(mr) == 0 ? "OK" : "FAIL");
    }
    if (buf) {
        free(buf);
        printf("STEP 10 FREE_BUFFER_OK\n");
    }
    if (pd) {
        printf("STEP 10 DEALLOC_PD_%s\n", ibv_dealloc_pd(pd) == 0 ? "OK" : "FAIL");
    }
    if (ctx) {
        printf("STEP 10 CLOSE_DEVICE_%s\n", ibv_close_device(ctx) == 0 ? "OK" : "FAIL");
    }

    return rc;
}

int main(void)
{
    struct ibv_device **dev_list = NULL;
    int num_devices = 0;
    int rc = 0;

    printf("RDMA_OBJECT_LIFECYCLE_START\n");

    /*
     * STEP 00: 枚举 verbs devices。
     *
     * 如果这里 count=0，说明程序和 libibverbs 可以运行，
     * 但当前还没有 provider-visible RDMA device。
     */
    dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list) {
        printf("STEP 00 GET_DEVICE_LIST_FAIL errno=%d message=%s\n", errno, strerror(errno));
        return 2;
    }

    printf("STEP 00 GET_DEVICE_LIST_OK count=%d\n", num_devices);
    if (num_devices == 0) {
        printf("NO_RDMA_DEVICES_FOUND\n");
        ibv_free_device_list(dev_list);
        printf("RDMA_OBJECT_LIFECYCLE_END status=NO_DEVICE\n");
        return 0;
    }

    for (int i = 0; i < num_devices; i++) {
        print_device_summary(dev_list[i], i);
    }

    rc = inspect_device(dev_list[0]);
    ibv_free_device_list(dev_list);

    printf("RDMA_OBJECT_LIFECYCLE_END status=%s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}
