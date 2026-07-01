#ifndef QP_LAB_H
#define QP_LAB_H

#include <stdint.h>
#include <infiniband/verbs.h>

#define QP_CQ_DEPTH 32
#define QP_QUEUE_DEPTH 16U

struct qp_endpoint {
    struct ibv_qp *qp;
    uint32_t psn;
};

/* 两个本地 RC 端点共享 context、PD、CQ、port 和 GID。 */
struct qp_lab {
    struct ibv_device **list;
    int count;
    struct ibv_device *device;
    struct ibv_context *context;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_port_attr port_attr;
    union ibv_gid gid;
    uint8_t port;
    int gid_index;
    struct qp_endpoint left;
    struct qp_endpoint right;
};

int qp_list_devices(void);
int qp_lab_open(struct qp_lab *lab, const char *name, uint8_t port, int gid_index);
int qp_create_pair(struct qp_lab *lab);
int qp_run_state_machine(struct qp_lab *lab);
int qp_run_invalid_transition(struct qp_lab *lab);
void qp_lab_close(struct qp_lab *lab);
const char *qp_state_name(enum ibv_qp_state state);

#endif
