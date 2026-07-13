#define _POSIX_C_SOURCE 200112L
#include "rdma_cs.h"
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *rdma_cs_role_name(enum rdma_cs_role role)
{
    return role == RDMA_CS_ROLE_SERVER ? "server" : "client";
}

static const char *rdma_cs_env_or_dash(const char *name)
{
    const char *value = getenv(name);

    return value != NULL && value[0] != '\0' ? value : "-";
}

static const char *rdma_cs_role_cpuset_env(enum rdma_cs_role role)
{
    return role == RDMA_CS_ROLE_SERVER ?
           "RDMA_SERVER_CPUSET" : "RDMA_CLIENT_CPUSET";
}

static const char *rdma_cs_role_numa_env(enum rdma_cs_role role)
{
    return role == RDMA_CS_ROLE_SERVER ?
           "RDMA_SERVER_NUMA_NODE" : "RDMA_CLIENT_NUMA_NODE";
}

static int rdma_cs_read_proc_status_value(const char *key, char *buf,
                                          size_t buf_size)
{
#ifdef __linux__
    FILE *fp = fopen("/proc/self/status", "r");
    char line[256];
    size_t key_len = strlen(key);

    if (fp == NULL) {
        snprintf(buf, buf_size, "unavailable");
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == ':') {
            char *value = line + key_len + 1;

            while (*value == ' ' || *value == '\t')
                value++;
            snprintf(buf, buf_size, "%s", value);
            buf[strcspn(buf, "\r\n")] = '\0';
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
#endif
    snprintf(buf, buf_size, "unavailable");
    return -1;
}

static int rdma_cs_current_cpu(void)
{
#ifdef __linux__
    FILE *fp = fopen("/proc/self/stat", "r");
    char line[1024];
    char *cursor;
    int field = 0;

    if (fp == NULL)
        return -1;
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    cursor = strrchr(line, ')');
    if (cursor == NULL || cursor[1] != ' ')
        return -1;
    cursor += 2;

    for (char *token = strtok(cursor, " "); token != NULL;
         token = strtok(NULL, " ")) {
        field++;
        if (field == 37)
            return (int)strtol(token, NULL, 10);
    }
#endif
    return -1;
}

void rdma_cs_options_init(struct rdma_cs_options *options)
{
    /*
     * 所有 server/client 共享同一套默认参数。
     * 这里默认使用 127.0.0.1，是为了第一版先验证“双进程工程模型”，
     * 不把跨主机网络、路由、MTU、防火墙等问题混进 RDMA verbs 学习里。
     */
    memset(options, 0, sizeof(*options));
    options->listen_addr = RDMA_CS_DEFAULT_LISTEN;
    options->server_addr = RDMA_CS_DEFAULT_SERVER;
    options->tcp_port = RDMA_CS_DEFAULT_PORT;
    options->device_name = RDMA_CS_DEFAULT_DEVICE;
    options->ib_port = RDMA_CS_DEFAULT_IB_PORT;
    options->gid_index = RDMA_CS_DEFAULT_GID_INDEX;
}

void rdma_cs_print_usage(const char *program, enum rdma_cs_role role)
{
    const char *peer_option = role == RDMA_CS_ROLE_SERVER ?
                                  "[--listen IP]" : "[--server IP]";

    printf("usage: %s %s [--port PORT] [--device NAME] [--ib-port N] "
           "[--gid-index N] [--control-plane-only] [--dry-run]\n",
           program, peer_option);
}

void rdma_cs_options_print(enum rdma_cs_role role, const char *mode,
                           const struct rdma_cs_options *options)
{
    const char *role_name = rdma_cs_role_name(role);

    /*
     * 这行日志是每次实验的“起点快照”：角色、模式、TCP 地址、
     * RDMA device、端口和 GID index 都集中打印，方便复盘。
     */
    printf("app_config role=%s mode=%s listen=%s server=%s port=%s "
           "device=%s ib_port=%d gid_index=%d flags=%s%s%s%s\n",
           role_name, mode, options->listen_addr, options->server_addr,
           options->tcp_port, options->device_name, options->ib_port,
           options->gid_index,
           options->wrong_addr ? "wrong_addr," : "",
           options->skip_recv ? "skip_recv," : "",
           options->disconnect_after_rts ? "disconnect_after_rts," : "",
           options->wrong_rkey ? "wrong_rkey," : "");
}

void rdma_cs_log_binding(enum rdma_cs_role role)
{
    char cpus_allowed[128];
    char mems_allowed[128];

    /*
     * 这里记录的是“进程最终跑在什么约束下”，而不只是脚本传了什么参数。
     * requested_* 来自环境变量，cpus_allowed/mems_allowed 来自内核视角，
     * current_cpu 则给出当前时刻实际落到哪个 CPU，三者结合起来最方便复盘。
     */
    rdma_cs_read_proc_status_value("Cpus_allowed_list", cpus_allowed,
                                   sizeof(cpus_allowed));
    rdma_cs_read_proc_status_value("Mems_allowed_list", mems_allowed,
                                   sizeof(mems_allowed));

    printf("app_runtime_binding role=%s requested_cpuset=%s requested_numa_node=%s current_cpu=%d cpus_allowed=%s mems_allowed=%s\n",
           rdma_cs_role_name(role),
           rdma_cs_env_or_dash(rdma_cs_role_cpuset_env(role)),
           rdma_cs_env_or_dash(rdma_cs_role_numa_env(role)),
           rdma_cs_current_cpu(), cpus_allowed, mems_allowed);
}

int rdma_cs_parse_common(int argc, char **argv, struct rdma_cs_options *options,
                         enum rdma_cs_role role)
{
    static const struct option long_options[] = {
        {"listen", required_argument, NULL, 'l'},
        {"server", required_argument, NULL, 's'},
        {"port", required_argument, NULL, 'p'},
        {"device", required_argument, NULL, 'd'},
        {"ib-port", required_argument, NULL, 'i'},
        {"gid-index", required_argument, NULL, 'g'},
        {"control-plane-only", no_argument, NULL, 'c'},
        {"dry-run", no_argument, NULL, 'n'},
        {"wrong-rkey", no_argument, NULL, 'w'},
        {"wrong-addr", no_argument, NULL, 'a'},
        {"skip-recv", no_argument, NULL, 'r'},
        {"disconnect-after-rts", no_argument, NULL, 'x'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    int option;

    /*
     * 这个解析函数被 server 和 client 共同使用：
     * - server 关心 --listen；
     * - client 关心 --server；
     * - 两边都关心 verbs device / ib-port / gid-index。
     *
     * 这样做的目的，是让控制面参数和 RDMA 参数在两个进程里保持一致，
     * 后续迁移到双机时只需要替换 IP/GID，不需要改代码结构。
     */
    rdma_cs_options_init(options);
    while ((option = getopt_long(argc, argv, "l:s:p:d:i:g:cnwarxh",
                                 long_options, NULL)) != -1) {
        switch (option) {
        case 'l':
            options->listen_addr = optarg;
            break;
        case 's':
            options->server_addr = optarg;
            break;
        case 'p':
            options->tcp_port = optarg;
            break;
        case 'd':
            options->device_name = optarg;
            break;
        case 'i':
            options->ib_port = atoi(optarg);
            break;
        case 'g':
            options->gid_index = atoi(optarg);
            break;
        case 'c':
            options->control_plane_only = 1;
            break;
        case 'n':
            options->dry_run = 1;
            break;
        case 'w':
            options->wrong_rkey = 1;
            break;
        case 'a':
            options->wrong_addr = 1;
            break;
        case 'r':
            options->skip_recv = 1;
            break;
        case 'x':
            options->disconnect_after_rts = 1;
            break;
        case 'h':
            rdma_cs_print_usage(argv[0], role);
            return 1;
        default:
            return -1;
        }
    }

    if (options->ib_port <= 0 || options->gid_index < 0) {
        fprintf(stderr, "invalid ib-port or gid-index\n");
        return -1;
    }
    return 0;
}

void rdma_cs_metadata_dummy(struct rdma_cs_metadata *metadata,
                            enum rdma_cs_role role)
{
    /*
     * control-plane-only 模式不创建 RDMA 资源。
     * 因此这里构造一份假的 metadata，用来单独验证 TCP 连接、
     * 行协议格式化、字段解析和双向交换是否正确。
     */
    memset(metadata, 0, sizeof(*metadata));
    snprintf(metadata->role, sizeof(metadata->role), "%s",
             role == RDMA_CS_ROLE_SERVER ? "server" : "client");
    metadata->qpn = role == RDMA_CS_ROLE_SERVER ? 123 : 456;
    metadata->psn = role == RDMA_CS_ROLE_SERVER ? 0x111111 : 0x222222;
    metadata->gid_index = RDMA_CS_DEFAULT_GID_INDEX;
    snprintf(metadata->gid, sizeof(metadata->gid),
             "00000000000000000000000000000000");
    metadata->addr = role == RDMA_CS_ROLE_SERVER ? 0x12345678ULL : 0x87654321ULL;
    metadata->rkey = role == RDMA_CS_ROLE_SERVER ? 0xabcdef01U : 0x10203040U;
}

int rdma_cs_metadata_format(const struct rdma_cs_metadata *metadata,
                            char *line, size_t line_size)
{
    /*
     * 控制面使用可读的一行文本，而不是二进制 struct。
     * 学习阶段这样更容易在日志里看清楚双方交换了什么；
     * 生产系统可以换成 protobuf/flatbuffers/自定义二进制协议。
     */
    int written = snprintf(line, line_size,
                           "role=%s qpn=%u psn=0x%x gid_index=%d "
                           "gid=%s addr=0x%llx rkey=0x%x\n",
                           metadata->role, metadata->qpn, metadata->psn,
                           metadata->gid_index, metadata->gid,
                           (unsigned long long)metadata->addr,
                           metadata->rkey);

    return written > 0 && (size_t)written < line_size ? 0 : -1;
}

static int parse_u32(const char *value, uint32_t *out)
{
    char *end = NULL;
    unsigned long parsed;

    /*
     * strtoul 支持 0x 前缀，所以 qpn=123 和 psn=0x111111 都能解析。
     * 必须检查 end 指针，否则 "123abc" 这种坏输入会被静默接受。
     */
    errno = 0;
    parsed = strtoul(value, &end, 0);
    if (errno != 0 || end == value || *end != '\0' || parsed > UINT32_MAX)
        return -1;
    *out = (uint32_t)parsed;
    return 0;
}

static int parse_u64(const char *value, uint64_t *out)
{
    char *end = NULL;
    unsigned long long parsed;

    errno = 0;
    parsed = strtoull(value, &end, 0);
    if (errno != 0 || end == value || *end != '\0')
        return -1;
    *out = (uint64_t)parsed;
    return 0;
}

int rdma_cs_metadata_parse(const char *line, struct rdma_cs_metadata *metadata)
{
    char copy[RDMA_CS_LINE_SIZE];
    char *token;
    unsigned int seen = 0;

    /*
     * seen 是字段存在位图：
     * bit0 role, bit1 qpn, bit2 psn, bit3 gid_index,
     * bit4 gid, bit5 addr, bit6 rkey。
     *
     * 只有所有字段都出现才认为 metadata 合法。RDMA RC 建链缺一个字段
     * 都可能导致 QP RTR 失败，或者更糟糕地把 RDMA WRITE 发到错误地址。
     */
    memset(metadata, 0, sizeof(*metadata));
    snprintf(copy, sizeof(copy), "%s", line);

    token = strtok(copy, " \t\r\n");
    while (token != NULL) {
        char *equals = strchr(token, '=');
        const char *key;
        const char *value;

        if (equals == NULL)
            return -1;
        *equals = '\0';
        key = token;
        value = equals + 1;

        if (strcmp(key, "role") == 0) {
            snprintf(metadata->role, sizeof(metadata->role), "%s", value);
            seen |= 1U << 0;
        } else if (strcmp(key, "qpn") == 0) {
            if (parse_u32(value, &metadata->qpn) != 0)
                return -1;
            seen |= 1U << 1;
        } else if (strcmp(key, "psn") == 0) {
            if (parse_u32(value, &metadata->psn) != 0)
                return -1;
            seen |= 1U << 2;
        } else if (strcmp(key, "gid_index") == 0) {
            metadata->gid_index = atoi(value);
            seen |= 1U << 3;
        } else if (strcmp(key, "gid") == 0) {
            if (strlen(value) != RDMA_CS_GID_HEX_SIZE - 1)
                return -1;
            snprintf(metadata->gid, sizeof(metadata->gid), "%s", value);
            seen |= 1U << 4;
        } else if (strcmp(key, "addr") == 0) {
            if (parse_u64(value, &metadata->addr) != 0)
                return -1;
            seen |= 1U << 5;
        } else if (strcmp(key, "rkey") == 0) {
            if (parse_u32(value, &metadata->rkey) != 0)
                return -1;
            seen |= 1U << 6;
        }
        token = strtok(NULL, " \t\r\n");
    }

    return seen == 0x7fU ? 0 : -1;
}

void rdma_cs_metadata_print(const char *prefix,
                            const struct rdma_cs_metadata *metadata)
{
    printf("%s role=%s qpn=%u psn=0x%x gid_index=%d gid=%s "
           "addr=0x%llx rkey=0x%x\n",
           prefix, metadata->role, metadata->qpn, metadata->psn,
           metadata->gid_index, metadata->gid,
           (unsigned long long)metadata->addr, metadata->rkey);
}
