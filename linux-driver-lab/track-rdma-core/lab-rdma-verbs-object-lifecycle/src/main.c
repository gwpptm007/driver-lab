#include "rdma_resources.h"

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(FILE *stream)
{
    /* 帮助信息也是测试依赖的稳定 CLI 契约，不要随意改变首行格式。 */
    fprintf(stream,
            "usage: rdma-lifecycle [--list] [--device NAME] [--port N]\n"
            "\n"
            "  --list          list provider-visible RDMA devices\n"
            "  --device NAME   select a device (default: first device)\n"
            "  --port N        select a physical port (default: 1)\n"
            "  --help          show this help\n");
}

static int parse_port(const char *text, uint8_t *port)
{
    char *end = NULL;
    unsigned long value;

    /*
     * strtoul() 同时可能通过 errno 和 end 指针报告错误。
     * 这里拒绝 0、尾随字符和超过 uint8_t 的值，避免发生静默截断。
     */
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        value == 0 || value > UCHAR_MAX) {
        return -EINVAL;
    }
    *port = (uint8_t)value;
    return 0;
}

int main(int argc, char **argv)
{
    /* getopt_long() 让短参数与长参数共享同一套解析分支。 */
    static const struct option long_options[] = {
        {"list", no_argument, NULL, 'l'},
        {"device", required_argument, NULL, 'd'},
        {"port", required_argument, NULL, 'p'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    struct rdma_resources res;
    const char *device_name = NULL;
    uint8_t port = RDMA_DEFAULT_PORT;
    int list_only = 0;
    int option;
    int rc = EXIT_FAILURE;

    /*
     * 必须先清零资源结构。
     * 后续任一步失败都可以进入 out，cleanup 通过 NULL 判断哪些对象已创建。
     */
    memset(&res, 0, sizeof(res));
    opterr = 0;

    /* 第一阶段：只解析和校验参数，此时还没有申请任何 RDMA 资源。 */
    while ((option = getopt_long(argc, argv, "ld:p:h", long_options, NULL)) != -1) {
        switch (option) {
        case 'l':
            list_only = 1;
            break;
        case 'd':
            device_name = optarg;
            break;
        case 'p':
            if (parse_port(optarg, &port) != 0) {
                fprintf(stderr, "error=invalid_port value=%s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'h':
            print_usage(stdout);
            return EXIT_SUCCESS;
        default:
            fprintf(stderr, "error=invalid_arguments\n");
            print_usage(stderr);
            return EXIT_FAILURE;
        }
    }
    if (optind != argc) {
        fprintf(stderr, "error=invalid_arguments\n");
        print_usage(stderr);
        return EXIT_FAILURE;
    }
    if (list_only) {
        /* list 模式只做设备发现，不创建 context/PD/MR/CQ/QP。 */
        return rdma_list_devices() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    /*
     * 第二阶段：设备层。
     * device 是静态描述，context 才是当前进程访问 provider/device 的会话。
     */
    if (rdma_device_open(&res, device_name, port) != 0) {
        goto out;
    }
    printf("device=%s\n", ibv_get_device_name(res.device));
    printf("port=%u state=%u active_mtu=%u\n",
           res.port_num, res.port_attr.state, res.port_attr.active_mtu);
    printf("context=ready\n");

    /*
     * 第三阶段：内存层。
     * 创建 PD、分配普通 buffer，再注册 MR 并获得 lkey/rkey。
     */
    if (rdma_memory_create(&res, RDMA_BUFFER_SIZE) != 0) {
        goto out;
    }
    printf("pd=ready\n");
    printf("mr=ready address=%p length=%zu lkey=0x%x rkey=0x%x\n",
           res.mr->addr, res.mr->length, res.mr->lkey, res.mr->rkey);

    /*
     * 第四阶段：队列层。
     * 此处只创建 CQ 和 RC QP；尚未配置对端信息，所以 QP 仍处于 RESET。
     */
    if (rdma_queue_create(&res) != 0) {
        goto out;
    }
    printf("cq=ready depth=%d\n", RDMA_CQ_DEPTH);
    printf("qp=ready qp_num=%u qp_type=RC\n", res.qp->qp_num);
    printf("qp_state=%s\n", rdma_qp_state_name(res.qp_state));
    rc = EXIT_SUCCESS;

out:
    /*
     * 唯一退出路径：无论失败发生在哪一层，都按依赖关系逆序释放。
     * 这是本项目最重要的工程模式，而不只是“防止内存泄漏”。
     */
    rdma_resources_cleanup(&res);
    printf("cleanup=complete\n");
    printf("result=%s\n", rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}
