#ifndef RDMA_RESOURCES_H
#define RDMA_RESOURCES_H

#include <stddef.h>
#include <stdint.h>

#include <infiniband/verbs.h>

/* 本实验只创建对象，不收发数据，因此队列和缓冲区规模保持最小且固定。 */
#define RDMA_DEFAULT_PORT 1U
#define RDMA_BUFFER_SIZE 4096U
#define RDMA_CQ_DEPTH 16
#define RDMA_QUEUE_DEPTH 16U
#define RDMA_MAX_SGE 1U

/*
 * 资源聚合结构明确记录 ownership。
 *
 * 只要资源创建成功，指针就存入这里；统一清理函数再按依赖关系逆序释放。
 * 这种写法比把资源散落在 main() 的局部变量里更接近真实 RDMA 工程。
 */
struct rdma_resources {
    /* 设备层：device 来自 device_list，释放列表前不能单独释放 device。 */
    struct ibv_device **device_list;
    int device_count;
    struct ibv_device *device;
    struct ibv_context *context;
    struct ibv_device_attr device_attr;
    struct ibv_port_attr port_attr;
    uint8_t port_num;

    /* 内存层：QP 与 MR 必须归属于同一个 PD，buffer 由 MR 覆盖。 */
    struct ibv_pd *pd;
    void *buffer;
    size_t buffer_length;
    struct ibv_mr *mr;

    /* 队列层：本实验让 SQ/RQ 共用一个 CQ，QP 创建后应处于 RESET。 */
    struct ibv_cq *cq;
    struct ibv_qp *qp;
    enum ibv_qp_state qp_state;
};

/* 设备模块：负责枚举设备，建立进程到设备的 context，并查询端口。 */
int rdma_list_devices(void);
int rdma_device_open(struct rdma_resources *res, const char *name, uint8_t port);

/* 内存模块：在 context 下建立 PD，并将普通用户内存注册为 MR。 */
int rdma_memory_create(struct rdma_resources *res, size_t length);

/* 队列模块：创建 CQ 和 RC QP，并查询 QP 的初始状态。 */
int rdma_queue_create(struct rdma_resources *res);

/*
 * 销毁接口允许对应资源尚未创建，因此可以安全处理“部分初始化”状态。
 * 统一清理顺序必须是：QP/CQ -> MR/buffer/PD -> context/device list。
 */
void rdma_queue_destroy(struct rdma_resources *res);
void rdma_memory_destroy(struct rdma_resources *res);
void rdma_device_close(struct rdma_resources *res);
void rdma_resources_cleanup(struct rdma_resources *res);

const char *rdma_qp_state_name(enum ibv_qp_state state);

#endif
