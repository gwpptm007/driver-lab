#include "qp_lab.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int qp_list_devices(void)
{
    struct ibv_device **list;
    int count = 0;
    int i;

    list = ibv_get_device_list(&count);
    if (list == NULL)
        return -1;
    printf("device_count=%d\n", count);
    for (i = 0; i < count; ++i)
        printf("device[%d]=%s\n", i, ibv_get_device_name(list[i]));
    ibv_free_device_list(list);
    return 0;
}

int qp_lab_open(struct qp_lab *lab, const char *name, uint8_t port, int gid_index)
{
    int i;

    lab->list = ibv_get_device_list(&lab->count);
    if (lab->list == NULL || lab->count == 0) {
        fprintf(stderr, "error=no_rdma_device\n");
        return -1;
    }
    for (i = 0; i < lab->count; ++i) {
        if (name == NULL || strcmp(name, ibv_get_device_name(lab->list[i])) == 0) {
            lab->device = lab->list[i];
            break;
        }
    }
    if (lab->device == NULL) {
        fprintf(stderr, "error=device_not_found\n");
        return -1;
    }

    /* context、PD、CQ 是两个 QP 共同依赖的上游对象。 */
    lab->context = ibv_open_device(lab->device);
    if (lab->context == NULL || ibv_query_port(lab->context, port, &lab->port_attr) != 0)
        return -1;
    if (gid_index < 0 || gid_index >= lab->port_attr.gid_tbl_len ||
        ibv_query_gid(lab->context, port, gid_index, &lab->gid) != 0)
        return -1;
    lab->port = port;
    lab->gid_index = gid_index;
    lab->pd = ibv_alloc_pd(lab->context);
    lab->cq = lab->pd ? ibv_create_cq(lab->context, QP_CQ_DEPTH, NULL, NULL, 0) : NULL;
    if (lab->pd == NULL || lab->cq == NULL) {
        fprintf(stderr, "error=create_shared_resources errno=%d message=%s\n", errno, strerror(errno));
        return -1;
    }
    return 0;
}

void qp_lab_close(struct qp_lab *lab)
{
    /* QP 引用 CQ/PD，因此严格按 QP -> CQ -> PD -> context 释放。 */
    if (lab->right.qp != NULL)
        ibv_destroy_qp(lab->right.qp);
    if (lab->left.qp != NULL)
        ibv_destroy_qp(lab->left.qp);
    if (lab->cq != NULL)
        ibv_destroy_cq(lab->cq);
    if (lab->pd != NULL)
        ibv_dealloc_pd(lab->pd);
    if (lab->context != NULL)
        ibv_close_device(lab->context);
    if (lab->list != NULL)
        ibv_free_device_list(lab->list);
    memset(lab, 0, sizeof(*lab));
}
