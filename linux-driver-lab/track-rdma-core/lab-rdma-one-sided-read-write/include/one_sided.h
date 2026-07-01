#ifndef ONE_SIDED_H
#define ONE_SIDED_H

#include <stdint.h>
#include <infiniband/verbs.h>

#define BUFFER_SIZE 256U
#define CQ_DEPTH 32

/* 一个通信端点拥有独立的 QP、MR、buffer 和发送起始 PSN。 */
struct endpoint {
    const char *name;
    struct ibv_qp *qp;
    struct ibv_mr *mr;
    char *buf;
    uint32_t psn;
};

/*
 * 两个端点共享同一个 device context、PD、CQ 和 RoCE GID。
 * 单进程双 QP 结构用于聚焦 one-sided address/rkey 与 CQE 语义。
 */
struct session {
    struct ibv_device **list;
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_port_attr port_attr;
    union ibv_gid gid;
    uint8_t port;
    int gid_index;
    struct endpoint left;
    struct endpoint right;
};

#endif
