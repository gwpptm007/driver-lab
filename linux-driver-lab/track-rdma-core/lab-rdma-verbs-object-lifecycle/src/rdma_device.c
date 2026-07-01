#include "rdma_resources.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int rdma_list_devices(void)
{
    struct ibv_device **list;
    int count = 0;
    int i;

    /*
     * 这里枚举的是 provider-visible RDMA device，而不是 ip link 的 net_device。
     * ens34 存在但 rxe0 未创建时，count 仍然可能为 0。
     */
    list = ibv_get_device_list(&count);
    if (list == NULL) {
        fprintf(stderr, "error=get_device_list errno=%d message=%s\n",
                errno, strerror(errno));
        return -errno;
    }

    printf("device_count=%d\n", count);
    for (i = 0; i < count; ++i) {
        printf("device[%d]=%s\n", i, ibv_get_device_name(list[i]));
    }

    /* list 中的 ibv_device 由 libibverbs 管理，统一释放整个列表。 */
    ibv_free_device_list(list);
    return 0;
}

int rdma_device_open(struct rdma_resources *res, const char *name, uint8_t port)
{
    int i;

    /*
     * 生命周期模式需要让选中的 device 一直有效到 context 关闭，
     * 所以 device_list 保存到 res 中，不能像 --list 模式那样立刻释放。
     */
    res->device_list = ibv_get_device_list(&res->device_count);
    if (res->device_list == NULL) {
        fprintf(stderr, "error=get_device_list errno=%d message=%s\n",
                errno, strerror(errno));
        return -errno;
    }
    if (res->device_count == 0) {
        if (name != NULL) {
            fprintf(stderr, "error=device_not_found device=%s\n", name);
        } else {
            fprintf(stderr, "error=no_rdma_device\n");
        }
        return -ENODEV;
    }

    /* 未指定名称时采用第一个设备；生产程序通常会要求显式选择。 */
    if (name == NULL) {
        res->device = res->device_list[0];
    } else {
        for (i = 0; i < res->device_count; ++i) {
            if (strcmp(name, ibv_get_device_name(res->device_list[i])) == 0) {
                res->device = res->device_list[i];
                break;
            }
        }
    }
    if (res->device == NULL) {
        fprintf(stderr, "error=device_not_found device=%s\n", name);
        return -ENODEV;
    }

    /*
     * 打开 device 会建立用户态 provider 与内核 uverbs 的进程级 context。
     * context 不是远端连接，真正的连接对象是后面创建并迁移状态的 QP。
     */
    res->context = ibv_open_device(res->device);
    if (res->context == NULL) {
        fprintf(stderr, "error=open_device device=%s errno=%d message=%s\n",
                ibv_get_device_name(res->device), errno, strerror(errno));
        return -errno;
    }

    /* 先查询设备能力，phys_port_cnt 用于校验用户指定端口是否存在。 */
    if (ibv_query_device(res->context, &res->device_attr) != 0) {
        fprintf(stderr, "error=query_device errno=%d message=%s\n",
                errno, strerror(errno));
        return -errno;
    }
    if (port == 0 || port > res->device_attr.phys_port_cnt) {
        fprintf(stderr, "error=invalid_port port=%u available_ports=%u\n",
                port, res->device_attr.phys_port_cnt);
        return -EINVAL;
    }
    /* port_attr 保存链路状态、active MTU、GID 表长度等运行时属性。 */
    if (ibv_query_port(res->context, port, &res->port_attr) != 0) {
        fprintf(stderr, "error=query_port port=%u errno=%d message=%s\n",
                port, errno, strerror(errno));
        return -errno;
    }

    res->port_num = port;
    return 0;
}

void rdma_device_close(struct rdma_resources *res)
{
    /* context 依赖 device，因此先关闭 context，最后释放 device_list。 */
    if (res->context != NULL) {
        if (ibv_close_device(res->context) != 0) {
            fprintf(stderr, "warning=close_device_failed errno=%d message=%s\n",
                    errno, strerror(errno));
        }
        res->context = NULL;
    }
    if (res->device_list != NULL) {
        ibv_free_device_list(res->device_list);
        res->device_list = NULL;
    }
    res->device = NULL;
    res->device_count = 0;
}
