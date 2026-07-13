#ifndef RDMA_CS_H
#define RDMA_CS_H

#include <infiniband/verbs.h>
#include <stdint.h>
#include <stddef.h>

#define RDMA_CS_DEFAULT_PORT "18515"
#define RDMA_CS_DEFAULT_LISTEN "127.0.0.1"
#define RDMA_CS_DEFAULT_SERVER "127.0.0.1"
#define RDMA_CS_DEFAULT_DEVICE "rxe0"
#define RDMA_CS_DEFAULT_IB_PORT 1
#define RDMA_CS_DEFAULT_GID_INDEX 1
#define RDMA_CS_BUFFER_SIZE 4096U
#define RDMA_CS_CQ_DEPTH 32
#define RDMA_CS_CQ_TIMEOUT_MS 5000
#define RDMA_CS_LINE_SIZE 256
#define RDMA_CS_GID_HEX_SIZE 33

enum rdma_cs_role {
    RDMA_CS_ROLE_SERVER = 1,
    RDMA_CS_ROLE_CLIENT = 2,
};

struct rdma_cs_options {
    const char *listen_addr;
    const char *server_addr;
    const char *tcp_port;
    const char *device_name;
    int ib_port;
    int gid_index;
    int control_plane_only;
    int dry_run;
    int wrong_rkey;
    int wrong_addr;
    int skip_recv;
    int disconnect_after_rts;
};

struct rdma_cs_metadata {
    char role[16];
    uint32_t qpn;
    uint32_t psn;
    int gid_index;
    char gid[RDMA_CS_GID_HEX_SIZE];
    uint64_t addr;
    uint32_t rkey;
};

struct rdma_cs_context {
    struct ibv_device **device_list;
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_qp *qp;
    struct ibv_mr *mr;
    struct ibv_port_attr port_attr;
    union ibv_gid gid;
    char *buf;
    uint32_t psn;
    uint8_t ib_port;
    int gid_index;
};

void rdma_cs_options_init(struct rdma_cs_options *options);
int rdma_cs_parse_common(int argc, char **argv, struct rdma_cs_options *options,
                         enum rdma_cs_role role);
void rdma_cs_print_usage(const char *program, enum rdma_cs_role role);
void rdma_cs_options_print(enum rdma_cs_role role, const char *mode,
                           const struct rdma_cs_options *options);
void rdma_cs_log_binding(enum rdma_cs_role role);

void rdma_cs_metadata_dummy(struct rdma_cs_metadata *metadata,
                            enum rdma_cs_role role);
int rdma_cs_metadata_from_context(struct rdma_cs_metadata *metadata,
                                  const struct rdma_cs_context *context,
                                  enum rdma_cs_role role);
int rdma_cs_metadata_format(const struct rdma_cs_metadata *metadata,
                            char *line, size_t line_size);
int rdma_cs_metadata_parse(const char *line, struct rdma_cs_metadata *metadata);
void rdma_cs_metadata_print(const char *prefix,
                            const struct rdma_cs_metadata *metadata);

int rdma_cs_tcp_listen(const char *host, const char *port);
int rdma_cs_tcp_accept(int listen_fd);
int rdma_cs_tcp_connect(const char *host, const char *port);
int rdma_cs_exchange_metadata(int fd,
                              const struct rdma_cs_metadata *local,
                              struct rdma_cs_metadata *remote);
int rdma_cs_send_line(int fd, const char *line);
int rdma_cs_recv_line(int fd, char *buf, size_t len);
void rdma_cs_close_fd(int fd);

int rdma_cs_resources_create(struct rdma_cs_context *context,
                             const struct rdma_cs_options *options,
                             uint32_t psn);
void rdma_cs_resources_destroy(struct rdma_cs_context *context);
void rdma_cs_gid_to_hex(const union ibv_gid *gid, char out[RDMA_CS_GID_HEX_SIZE]);
int rdma_cs_gid_from_hex(const char *hex, union ibv_gid *gid);
int rdma_cs_qp_to_rts(struct rdma_cs_context *context,
                      const struct rdma_cs_metadata *remote);
int rdma_cs_post_recv(struct rdma_cs_context *context, uint64_t wr_id);
int rdma_cs_post_send(struct rdma_cs_context *context, const char *payload,
                      uint64_t wr_id);
int rdma_cs_post_send_flags(struct rdma_cs_context *context, const char *payload,
                            uint64_t wr_id, int send_flags);
int rdma_cs_post_write(struct rdma_cs_context *context,
                       const struct rdma_cs_metadata *remote,
                       const char *payload, uint64_t wr_id,
                       int wrong_rkey, int wrong_addr);
int rdma_cs_post_read(struct rdma_cs_context *context,
                      const struct rdma_cs_metadata *remote,
                      size_t length, uint64_t wr_id);
int rdma_cs_poll_success(struct rdma_cs_context *context, const char *tag);
int rdma_cs_poll_expect_error(struct rdma_cs_context *context, const char *tag);

#endif
