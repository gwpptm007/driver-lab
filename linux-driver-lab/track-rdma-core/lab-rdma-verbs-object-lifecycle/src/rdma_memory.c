#define _POSIX_C_SOURCE 200112L

#include "rdma_resources.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int rdma_memory_create(struct rdma_resources *res, size_t length)
{
    int rc;

    /* PD 是 QP 与 MR 的共同保护边界，两类对象必须属于同一个 PD。 */
    res->pd = ibv_alloc_pd(res->context);
    if (res->pd == NULL) {
        fprintf(stderr, "error=alloc_pd errno=%d message=%s\n",
                errno, strerror(errno));
        return -errno;
    }

    /*
     * 页对齐不是 ibv_reg_mr() 的通用硬性要求，但便于观察地址与页边界，
     * 也更接近 DMA/固定页相关实验中常见的缓冲区布局。
     */
    rc = posix_memalign(&res->buffer, 4096, length);
    if (rc != 0) {
        fprintf(stderr, "error=alloc_buffer errno=%d message=%s\n",
                rc, strerror(rc));
        return -rc;
    }
    res->buffer_length = length;
    memset(res->buffer, 0, length);

    /*
     * 注册 MR 后 provider 建立 DMA/页固定相关映射并返回访问 key。
     * lkey 给本地 SGE 使用，rkey 将来用于授权远端 READ/WRITE。
     */
    res->mr = ibv_reg_mr(res->pd, res->buffer, res->buffer_length,
                         IBV_ACCESS_LOCAL_WRITE |
                         IBV_ACCESS_REMOTE_READ |
                         IBV_ACCESS_REMOTE_WRITE);
    if (res->mr == NULL) {
        fprintf(stderr, "error=register_mr errno=%d message=%s\n",
                errno, strerror(errno));
        return -errno;
    }

    return 0;
}

void rdma_memory_destroy(struct rdma_resources *res)
{
    /*
     * MR 引用 buffer 和 PD，因此必须先注销 MR，再释放内存，最后销毁 PD。
     * 每个判断都支持前序创建失败后的“部分初始化”回滚。
     */
    if (res->mr != NULL) {
        if (ibv_dereg_mr(res->mr) != 0) {
            fprintf(stderr, "warning=deregister_mr_failed errno=%d message=%s\n",
                    errno, strerror(errno));
        }
        res->mr = NULL;
    }
    free(res->buffer);
    res->buffer = NULL;
    res->buffer_length = 0;

    if (res->pd != NULL) {
        if (ibv_dealloc_pd(res->pd) != 0) {
            fprintf(stderr, "warning=dealloc_pd_failed errno=%d message=%s\n",
                    errno, strerror(errno));
        }
        res->pd = NULL;
    }
}
