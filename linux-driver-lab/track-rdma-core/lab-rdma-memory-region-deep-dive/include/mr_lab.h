#ifndef MR_LAB_H
#define MR_LAB_H

#include <stdint.h>
#include <infiniband/verbs.h>

#define MR_TEST_SIZE 8192U

/* 本项目只保留 MR 实验真正需要的上游资源。 */
struct mr_environment {
    struct ibv_device **device_list;
    int device_count;
    struct ibv_device *device;
    struct ibv_context *context;
    struct ibv_device_attr device_attr;
    struct ibv_port_attr port_attr;
    struct ibv_pd *pd;
    uint8_t port;
};

int mr_list_devices(void);
int mr_environment_open(struct mr_environment *env, const char *name, uint8_t port);
void mr_environment_close(struct mr_environment *env);
int mr_run_suite(struct mr_environment *env);

#endif
