#include "mr_lab.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *out)
{
    fprintf(out, "usage: rdma-mr-lab [--list] [--device NAME] [--port N] [--help]\n");
}

int main(int argc, char **argv)
{
    static const struct option options[] = {
        {"list", no_argument, NULL, 'l'},
        {"device", required_argument, NULL, 'd'},
        {"port", required_argument, NULL, 'p'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    struct mr_environment env;
    const char *device = NULL;
    unsigned long port_value = 1;
    int list = 0;
    int opt;
    int rc = EXIT_FAILURE;

    memset(&env, 0, sizeof(env));
    while ((opt = getopt_long(argc, argv, "ld:p:h", options, NULL)) != -1) {
        switch (opt) {
        case 'l': list = 1; break;
        case 'd': device = optarg; break;
        case 'p':
            errno = 0;
            port_value = strtoul(optarg, NULL, 10);
            if (errno != 0 || port_value == 0 || port_value > 255) {
                fprintf(stderr, "error=invalid_port value=%s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'h': usage(stdout); return EXIT_SUCCESS;
        default: usage(stderr); return EXIT_FAILURE;
        }
    }
    if (list)
        return mr_list_devices() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    /* 主流程只负责环境生命周期，具体 MR 差异全部由实验 case 表驱动。 */
    if (mr_environment_open(&env, device, (uint8_t)port_value) != 0)
        goto out;
    printf("device=%s port=%u pd=ready\n", ibv_get_device_name(env.device), env.port);
    rc = mr_run_suite(&env) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

out:
    mr_environment_close(&env);
    printf("cleanup=complete result=%s\n", rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}
