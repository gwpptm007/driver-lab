#ifndef UD_LAB_H
#define UD_LAB_H

#include <stdint.h>
#include <infiniband/verbs.h>

#define UD_BUFFER_SIZE 256U
#define UD_GRH_SIZE 40U
#define UD_QKEY 0x11111111U

struct ud_endpoint {
    const char *name;
    struct ibv_qp *qp;
    struct ibv_mr *mr;
    unsigned char *buffer;
    uint32_t psn;
};

struct ud_session {
    struct ibv_device **list;
    struct ibv_context *context;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_ah *address_handle;
    union ibv_gid gid;
    uint8_t port;
    int gid_index;
    struct ud_endpoint sender;
    struct ud_endpoint receiver;
};

#endif
