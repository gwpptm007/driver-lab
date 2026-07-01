#include "mr_lab.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int mr_list_devices(void)
{
    struct ibv_device **list;
    int count = 0;
    int i;

    /* verbs device 与普通 net_device 不同，必须能被 provider 发现。 */
    list = ibv_get_device_list(&count);
    if (list == NULL) {
        fprintf(stderr, "error=get_device_list errno=%d message=%s\n", errno, strerror(errno));
        return -1;
    }
    printf("device_count=%d\n", count);
    for (i = 0; i < count; ++i)
        printf("device[%d]=%s\n", i, ibv_get_device_name(list[i]));
    ibv_free_device_list(list);
    return 0;
}

int mr_environment_open(struct mr_environment *env, const char *name, uint8_t port)
{
    int i;

    env->device_list = ibv_get_device_list(&env->device_count);
    if (env->device_list == NULL || env->device_count == 0) {
        fprintf(stderr, "error=no_rdma_device\n");
        return -1;
    }
    for (i = 0; i < env->device_count; ++i) {
        if (name == NULL || strcmp(name, ibv_get_device_name(env->device_list[i])) == 0) {
            env->device = env->device_list[i];
            break;
        }
    }
    if (env->device == NULL) {
        fprintf(stderr, "error=device_not_found device=%s\n", name);
        return -1;
    }

    /* context 建立进程与 provider/device 的会话，PD 则建立资源隔离域。 */
    env->context = ibv_open_device(env->device);
    if (env->context == NULL || ibv_query_device(env->context, &env->device_attr) != 0) {
        fprintf(stderr, "error=open_or_query_device errno=%d message=%s\n", errno, strerror(errno));
        return -1;
    }
    if (port == 0 || port > env->device_attr.phys_port_cnt ||
        ibv_query_port(env->context, port, &env->port_attr) != 0) {
        fprintf(stderr, "error=invalid_or_unavailable_port port=%u\n", port);
        return -1;
    }
    env->port = port;
    env->pd = ibv_alloc_pd(env->context);
    if (env->pd == NULL) {
        fprintf(stderr, "error=alloc_pd errno=%d message=%s\n", errno, strerror(errno));
        return -1;
    }
    return 0;
}

void mr_environment_close(struct mr_environment *env)
{
    /* MR case 必须先自行注销全部 MR，之后才能销毁 PD 和 context。 */
    if (env->pd != NULL) {
        ibv_dealloc_pd(env->pd);
        env->pd = NULL;
    }
    if (env->context != NULL) {
        ibv_close_device(env->context);
        env->context = NULL;
    }
    if (env->device_list != NULL) {
        ibv_free_device_list(env->device_list);
        env->device_list = NULL;
    }
}
