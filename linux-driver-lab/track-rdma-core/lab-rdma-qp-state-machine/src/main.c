#include "qp_lab.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    puts("usage: rdma-qp-state [--list] [--device NAME] [--port N] [--gid-index N]");
}

int main(int argc, char **argv)
{
    static const struct option options[] = {
        {"list", no_argument, NULL, 'l'},
        {"device", required_argument, NULL, 'd'},
        {"port", required_argument, NULL, 'p'},
        {"gid-index", required_argument, NULL, 'g'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    struct qp_lab lab;
    const char *device = NULL;
    int port = 1, gid_index = 0, list = 0, opt, rc = EXIT_FAILURE;

    memset(&lab, 0, sizeof(lab));
    while ((opt = getopt_long(argc, argv, "ld:p:g:h", options, NULL)) != -1) {
        switch (opt) {
        case 'l': list = 1; break;
        case 'd': device = optarg; break;
        case 'p': port = atoi(optarg); break;
        case 'g': gid_index = atoi(optarg); break;
        case 'h': usage(); return EXIT_SUCCESS;
        default: usage(); return EXIT_FAILURE;
        }
    }
    if (list)
        return qp_list_devices() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    if (port < 1 || port > 255 || gid_index < 0 ||
        qp_lab_open(&lab, device, (uint8_t)port, gid_index) != 0 ||
        qp_create_pair(&lab) != 0)
        goto out;

    printf("device=%s port=%d gid_index=%d\n",
           ibv_get_device_name(lab.device), port, gid_index);
    if (qp_run_invalid_transition(&lab) != 0 || qp_run_state_machine(&lab) != 0)
        goto out;
    puts("state_machine_result=pass");
    rc = EXIT_SUCCESS;

out:
    qp_lab_close(&lab);
    printf("cleanup=complete result=%s\n", rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}
