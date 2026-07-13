#define _POSIX_C_SOURCE 200112L
#include "rdma_cs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t record_checksum(const struct rdma_kv_record *record)
{
    const unsigned char *key = (const unsigned char *)record->key;
    const unsigned char *value = (const unsigned char *)record->value;
    uint32_t hash = 2166136261U;
    size_t index;

    /* 覆盖固定长度字段，避免只比较字符串而漏掉尾部残留数据。 */
    for (index = 0; index < RDMA_KV_KEY_SIZE; ++index)
        hash = (hash ^ key[index]) * 16777619U;
    for (index = 0; index < RDMA_KV_VALUE_SIZE; ++index)
        hash = (hash ^ value[index]) * 16777619U;
    return (hash ^ record->version) * 16777619U;
}

static void build_expected_record(struct rdma_kv_record *record)
{
    memset(record, 0, sizeof(*record));
    snprintf(record->key, sizeof(record->key), "client-key-%u",
             RDMA_KV_TEST_SLOT);
    snprintf(record->value, sizeof(record->value),
             "client-value-write-read-slot-%u", RDMA_KV_TEST_SLOT);
    record->version = 1;
    record->checksum = record_checksum(record);
}

static void build_batch_records(struct rdma_kv_record *records)
{
    unsigned int index;

    for (index = 0; index < RDMA_KV_BATCH_CREDIT; ++index) {
        unsigned int slot = RDMA_KV_BATCH_FIRST_SLOT + index;

        memset(&records[index], 0, sizeof(records[index]));
        snprintf(records[index].key, sizeof(records[index].key),
                 "client-credit-key-%u", slot);
        snprintf(records[index].value, sizeof(records[index].value),
                 "client-credit-value-slot-%u", slot);
        records[index].version = 2;
        records[index].checksum = record_checksum(&records[index]);
    }
}

static void build_dynamic_record(struct rdma_kv_record *record,
                                 const char *key)
{
    memset(record, 0, sizeof(*record));
    snprintf(record->key, sizeof(record->key), "%s", key);
    snprintf(record->value, sizeof(record->value),
             "dynamic-value-for-%s", key);
    record->version = 3;
    record->checksum = record_checksum(record);
}

static int find_collision_key(char *out, size_t out_size, uint32_t bucket)
{
    unsigned int index;

    /* 测试运行时搜索同 bucket key，避免把碰撞样例绑定到人工常量。 */
    for (index = 0; index < 10000; ++index) {
        snprintf(out, out_size, "dynamic-collision-%u", index);
        if (rdma_kv_key_hash(out) % RDMA_KV_DIRECTORY_ENTRY_COUNT == bucket)
            return 0;
    }
    return -1;
}

static int expect_line(int fd, char *line, size_t line_size,
                       const char *expected)
{
    return rdma_cs_recv_line(fd, line, line_size) == 0 &&
           strcmp(line, expected) == 0 ? 0 : -1;
}

static int run_full_client(const struct rdma_cs_options *options)
{
    struct rdma_cs_context context;
    struct rdma_cs_metadata local;
    struct rdma_cs_metadata remote;
    struct rdma_kv_record expected;
    struct rdma_kv_record batch_expected[RDMA_KV_BATCH_CREDIT];
    struct rdma_kv_record dynamic_expected;
    struct rdma_kv_record rotated_expected;
    struct rdma_kv_directory_entry directory_entry;
    struct rdma_kv_directory_entry directory_read;
    struct rdma_kv_record *read_record;
    uint64_t remote_slot_addr;
    uint64_t remote_batch_addr;
    uint64_t remote_credit_addr;
    uint64_t remote_directory_addr;
    uint64_t remote_dynamic_record_addr;
    uint64_t remote_rotated_record_addr;
    uint64_t rotated_addr;
    uint64_t atomic_old;
    uint64_t acquire_add;
    uint32_t dynamic_hash;
    uint32_t dynamic_bucket;
    uint32_t old_remote_rkey;
    uint32_t rotated_rkey;
    unsigned long long parsed_addr;
    unsigned int parsed_rkey;
    const char *dynamic_key = "dynamic-alpha";
    char collision_key[RDMA_KV_KEY_SIZE];
    char line[RDMA_CS_LINE_SIZE];
    int fd = -1;
    int rc = EXIT_FAILURE;

    /* TCP 只负责阶段同步；KV 内容完全通过 one-sided WR 搬运。 */
    rdma_cs_options_print(RDMA_CS_ROLE_CLIENT, "one-sided-kv", options);
    rdma_cs_log_binding(RDMA_CS_ROLE_CLIENT);
    build_expected_record(&expected);
    build_batch_records(batch_expected);
    build_dynamic_record(&dynamic_expected, dynamic_key);

    if (rdma_cs_resources_create(&context, options, 0x4b564331U) != 0 ||
        rdma_cs_metadata_from_context(&local, &context,
                                      RDMA_CS_ROLE_CLIENT) != 0)
        goto out;

    fd = rdma_cs_tcp_connect(options->server_addr, options->tcp_port);
    if (fd < 0 || rdma_cs_exchange_metadata(fd, &local, &remote) != 0 ||
        rdma_cs_qp_to_rts(&context, &remote) != 0)
        goto out;

    remote_slot_addr = remote.addr +
                       (uint64_t)RDMA_KV_TEST_SLOT * sizeof(expected);
    remote_batch_addr = remote.addr +
                        (uint64_t)RDMA_KV_BATCH_FIRST_SLOT * sizeof(expected);
    remote_credit_addr = remote.addr + RDMA_KV_ATOMIC_CREDIT_OFFSET;
    acquire_add = 0U - (uint64_t)RDMA_KV_BATCH_CREDIT;
    printf("kv_remote_slot slot=%u addr=0x%llx rkey=0x%x\n",
           RDMA_KV_TEST_SLOT, (unsigned long long)remote_slot_addr,
           remote.rkey);
    puts("KV_QP_RTS_PASS");

    if (expect_line(fd, line, sizeof(line), "KV_WRITE_READY\n") != 0)
        goto out;
    puts("phase=kv_write role=client status=start");
    if (rdma_cs_post_write_at(&context, &expected, sizeof(expected),
                              remote_slot_addr, remote.rkey, 1001) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_write_cqe") != 0 ||
        rdma_cs_send_line(fd, "KV_WRITE_DONE\n") != 0)
        goto out;
    puts("phase=kv_write role=client status=done");
    puts("KV_WRITE_PASS");

    if (expect_line(fd, line, sizeof(line), "KV_READ_READY\n") != 0)
        goto out;
    puts("phase=kv_read role=client status=start");
    memset(context.buf, 0, sizeof(expected));
    if (rdma_cs_post_read_at(&context, sizeof(expected), remote_slot_addr,
                             remote.rkey, 1002) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_read_cqe") != 0)
        goto out;
    read_record = (struct rdma_kv_record *)context.buf;
    if (memcmp(read_record, &expected, sizeof(expected)) != 0 ||
        read_record->checksum != record_checksum(read_record) ||
        rdma_cs_send_line(fd, "KV_READ_DONE\n") != 0)
        goto out;
    printf("kv_read_record slot=%u key=%s value=%s version=%u checksum=0x%x\n",
           RDMA_KV_TEST_SLOT, read_record->key, read_record->value,
           read_record->version, read_record->checksum);
    puts("phase=kv_read role=client status=done");
    puts("KV_READ_PASS");

    /* credit 由控制面授予；本阶段不把 TCP token 伪装成远端原子计数器。 */
    if (expect_line(fd, line, sizeof(line), "KV_CREDIT grant=4\n") != 0 ||
        expect_line(fd, line, sizeof(line), "KV_ATOMIC_CREDIT_READY\n") != 0)
        goto out;
    printf("kv_credit_grant=%u\n", RDMA_KV_BATCH_CREDIT);
    puts("KV_CREDIT_PASS");

    if (rdma_cs_post_fetch_add(&context, remote_credit_addr, remote.rkey,
                               acquire_add, 1901) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_atomic_acquire_cqe") != 0)
        goto out;
    atomic_old = *(uint64_t *)context.buf;
    printf("kv_atomic_acquire old=%llu add=-%u\n",
           (unsigned long long)atomic_old, RDMA_KV_BATCH_CREDIT);
    if (atomic_old != RDMA_KV_BATCH_CREDIT ||
        rdma_cs_send_line(fd, "KV_ATOMIC_ACQUIRED old=4\n") != 0 ||
        expect_line(fd, line, sizeof(line), "KV_BATCH_WRITE_READY\n") != 0)
        goto out;
    puts("KV_REMOTE_ATOMIC_ACQUIRE_PASS");

    puts("phase=kv_batch_write role=client status=start");
    if (rdma_cs_post_write_batch_at(&context, batch_expected,
                                    sizeof(batch_expected[0]),
                                    RDMA_KV_BATCH_CREDIT, remote_batch_addr,
                                    remote.rkey, 2001) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_batch_write_tail_cqe") != 0 ||
        rdma_cs_send_line(fd, "KV_BATCH_WRITE_DONE count=4\n") != 0)
        goto out;
    puts("phase=kv_batch_write role=client status=done");
    puts("KV_BATCH_WRITE_PASS");

    if (expect_line(fd, line, sizeof(line), "KV_BATCH_READ_READY\n") != 0)
        goto out;
    memset(context.buf, 0, sizeof(batch_expected));
    puts("phase=kv_batch_read role=client status=start");
    if (rdma_cs_post_read_batch_at(&context, sizeof(batch_expected[0]),
                                   RDMA_KV_BATCH_CREDIT, remote_batch_addr,
                                   remote.rkey, 2005) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_batch_read_tail_cqe") != 0 ||
        memcmp(context.buf, batch_expected, sizeof(batch_expected)) != 0 ||
        rdma_cs_send_line(fd, "KV_BATCH_READ_DONE count=4\n") != 0)
        goto out;
    printf("kv_batch_read first_slot=%u count=%u tail_key=%s\n",
           RDMA_KV_BATCH_FIRST_SLOT, RDMA_KV_BATCH_CREDIT,
           ((struct rdma_kv_record *)context.buf)[RDMA_KV_BATCH_CREDIT - 1].key);
    puts("phase=kv_batch_read role=client status=done");
    puts("KV_BATCH_READ_PASS");

    if (expect_line(fd, line, sizeof(line), "KV_ATOMIC_RETURN_READY\n") != 0 ||
        rdma_cs_post_fetch_add(&context, remote_credit_addr, remote.rkey,
                               RDMA_KV_BATCH_CREDIT, 2901) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_atomic_return_cqe") != 0)
        goto out;
    atomic_old = *(uint64_t *)context.buf;
    printf("kv_atomic_return old=%llu add=%u\n",
           (unsigned long long)atomic_old, RDMA_KV_BATCH_CREDIT);
    if (atomic_old != 0 ||
        rdma_cs_send_line(fd, "KV_ATOMIC_RETURNED old=0\n") != 0)
        goto out;
    puts("KV_REMOTE_ATOMIC_RETURN_PASS");
    puts("KV_REMOTE_ATOMIC_CREDIT_PASS");

    /*
     * 用同一 QP 模拟两个逻辑竞争者：第二次 CAS 看到旧值 0，说明它没有
     * 覆盖持有者状态；归还后重试才允许把 4 原子地改为 0。
     */
    if (expect_line(fd, line, sizeof(line), "KV_CAS_HOLDER_READY\n") != 0 ||
        rdma_cs_post_compare_swap(&context, remote_credit_addr, remote.rkey,
                                  RDMA_KV_BATCH_CREDIT, 0, 3901) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_cas_holder_cqe") != 0)
        goto out;
    atomic_old = *(uint64_t *)context.buf;
    printf("kv_cas_holder old=%llu compare=%u swap=0\n",
           (unsigned long long)atomic_old, RDMA_KV_BATCH_CREDIT);
    if (atomic_old != RDMA_KV_BATCH_CREDIT ||
        rdma_cs_send_line(fd, "KV_CAS_HOLDER_ACQUIRED old=4\n") != 0)
        goto out;
    puts("KV_CAS_HOLDER_ACQUIRE_PASS");

    if (expect_line(fd, line, sizeof(line), "KV_CAS_CONTENDER_READY\n") != 0 ||
        rdma_cs_post_compare_swap(&context, remote_credit_addr, remote.rkey,
                                  RDMA_KV_BATCH_CREDIT, 0, 3902) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_cas_contender_cqe") != 0)
        goto out;
    atomic_old = *(uint64_t *)context.buf;
    printf("kv_cas_contender old=%llu compare=%u swap=0\n",
           (unsigned long long)atomic_old, RDMA_KV_BATCH_CREDIT);
    if (atomic_old != 0 ||
        rdma_cs_send_line(fd, "KV_CAS_CONTENDER_REJECTED old=0\n") != 0)
        goto out;
    puts("KV_CAS_CONTENDER_REJECT_PASS");

    if (expect_line(fd, line, sizeof(line), "KV_CAS_HOLDER_RETURN_READY\n") != 0 ||
        rdma_cs_post_fetch_add(&context, remote_credit_addr, remote.rkey,
                               RDMA_KV_BATCH_CREDIT, 3903) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_cas_holder_return_cqe") != 0)
        goto out;
    atomic_old = *(uint64_t *)context.buf;
    if (atomic_old != 0 ||
        rdma_cs_send_line(fd, "KV_CAS_HOLDER_RETURNED old=0\n") != 0)
        goto out;

    if (expect_line(fd, line, sizeof(line), "KV_CAS_RETRY_READY\n") != 0 ||
        rdma_cs_post_compare_swap(&context, remote_credit_addr, remote.rkey,
                                  RDMA_KV_BATCH_CREDIT, 0, 3904) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_cas_retry_cqe") != 0)
        goto out;
    atomic_old = *(uint64_t *)context.buf;
    printf("kv_cas_retry old=%llu compare=%u swap=0\n",
           (unsigned long long)atomic_old, RDMA_KV_BATCH_CREDIT);
    if (atomic_old != RDMA_KV_BATCH_CREDIT ||
        rdma_cs_send_line(fd, "KV_CAS_RETRY_ACQUIRED old=4\n") != 0)
        goto out;
    puts("KV_CAS_RETRY_ACQUIRE_PASS");

    if (expect_line(fd, line, sizeof(line), "KV_CAS_RETRY_RETURN_READY\n") != 0 ||
        rdma_cs_post_fetch_add(&context, remote_credit_addr, remote.rkey,
                               RDMA_KV_BATCH_CREDIT, 3905) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_cas_retry_return_cqe") != 0)
        goto out;
    atomic_old = *(uint64_t *)context.buf;
    if (atomic_old != 0 ||
        rdma_cs_send_line(fd, "KV_CAS_RETRY_RETURNED old=0\n") != 0)
        goto out;
    puts("KV_CAS_CREDIT_RECOVERY_PASS");
    puts("KV_CAS_CONTENTION_PASS");

    dynamic_hash = rdma_kv_key_hash(dynamic_key);
    dynamic_bucket = dynamic_hash % RDMA_KV_DIRECTORY_ENTRY_COUNT;
    remote_directory_addr = remote.addr + RDMA_KV_DIRECTORY_OFFSET +
        (uint64_t)dynamic_bucket * sizeof(directory_entry);
    remote_dynamic_record_addr = remote.addr +
        (uint64_t)dynamic_bucket * sizeof(dynamic_expected);
    memset(&directory_entry, 0, sizeof(directory_entry));
    directory_entry.hash = dynamic_hash;
    directory_entry.slot = dynamic_bucket;
    directory_entry.version = dynamic_expected.version;
    directory_entry.state = RDMA_KV_DIRECTORY_OCCUPIED;
    snprintf(directory_entry.key, sizeof(directory_entry.key), "%s",
             dynamic_key);

    if (expect_line(fd, line, sizeof(line), "KV_DIRECTORY_PUT_READY\n") != 0 ||
        rdma_cs_post_read_at(&context, sizeof(directory_read),
                             remote_directory_addr, remote.rkey, 4901) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_directory_probe_cqe") != 0)
        goto out;
    memcpy(&directory_read, context.buf, sizeof(directory_read));
    if (directory_read.state != RDMA_KV_DIRECTORY_EMPTY)
        goto out;

    /* 先提交 value，再发布目录项，读者不会看到尚未落盘的 value。 */
    if (rdma_cs_post_write_at(&context, &dynamic_expected,
                              sizeof(dynamic_expected),
                              remote_dynamic_record_addr, remote.rkey,
                              4902) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_dynamic_value_write_cqe") != 0 ||
        rdma_cs_post_write_at(&context, &directory_entry,
                              sizeof(directory_entry),
                              remote_directory_addr, remote.rkey, 4903) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_directory_publish_cqe") != 0 ||
        rdma_cs_send_line(fd, "KV_DIRECTORY_PUT_DONE\n") != 0)
        goto out;
    printf("kv_directory_put key=%s hash=0x%x bucket=%u slot=%u\n",
           dynamic_key, dynamic_hash, dynamic_bucket, directory_entry.slot);
    puts("KV_DYNAMIC_KEY_PUT_PASS");

    if (expect_line(fd, line, sizeof(line), "KV_DIRECTORY_GET_READY\n") != 0 ||
        rdma_cs_post_read_at(&context, sizeof(directory_read),
                             remote_directory_addr, remote.rkey, 4904) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_directory_lookup_cqe") != 0)
        goto out;
    memcpy(&directory_read, context.buf, sizeof(directory_read));
    if (directory_read.state != RDMA_KV_DIRECTORY_OCCUPIED ||
        directory_read.hash != dynamic_hash ||
        directory_read.slot != dynamic_bucket ||
        strcmp(directory_read.key, dynamic_key) != 0 ||
        rdma_cs_post_read_at(&context, sizeof(dynamic_expected),
                             remote_dynamic_record_addr, remote.rkey,
                             4905) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_dynamic_value_read_cqe") != 0 ||
        memcmp(context.buf, &dynamic_expected, sizeof(dynamic_expected)) != 0 ||
        rdma_cs_send_line(fd, "KV_DIRECTORY_GET_DONE\n") != 0)
        goto out;
    printf("kv_directory_get key=%s bucket=%u value=%s\n", dynamic_key,
           dynamic_bucket, ((struct rdma_kv_record *)context.buf)->value);
    puts("KV_DYNAMIC_KEY_GET_PASS");

    if (find_collision_key(collision_key, sizeof(collision_key),
                           dynamic_bucket) != 0 ||
        strcmp(collision_key, dynamic_key) == 0 ||
        expect_line(fd, line, sizeof(line),
                    "KV_DIRECTORY_COLLISION_READY\n") != 0 ||
        rdma_cs_post_read_at(&context, sizeof(directory_read),
                             remote_directory_addr, remote.rkey, 4906) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_collision_probe_cqe") != 0)
        goto out;
    memcpy(&directory_read, context.buf, sizeof(directory_read));
    if (directory_read.state != RDMA_KV_DIRECTORY_OCCUPIED ||
        strcmp(directory_read.key, collision_key) == 0 ||
        strcmp(directory_read.key, dynamic_key) != 0 ||
        rdma_cs_send_line(fd, "KV_DIRECTORY_COLLISION_REJECTED\n") != 0)
        goto out;
    printf("kv_directory_collision existing=%s rejected=%s bucket=%u\n",
           directory_read.key, collision_key, dynamic_bucket);
    puts("KV_DIRECTORY_COLLISION_PASS");
    puts("KV_DYNAMIC_DIRECTORY_PASS");

    old_remote_rkey = remote.rkey;
    if (rdma_cs_recv_line(fd, line, sizeof(line)) != 0 ||
        sscanf(line, "KV_RKEY_ROTATED addr=0x%llx rkey=0x%x\n",
               &parsed_addr, &parsed_rkey) != 2)
        goto out;
    rotated_addr = (uint64_t)parsed_addr;
    rotated_rkey = (uint32_t)parsed_rkey;
    if (rotated_addr != remote.addr || rotated_rkey == old_remote_rkey)
        goto out;
    remote_rotated_record_addr = rotated_addr +
        (uint64_t)RDMA_KV_ROTATION_SLOT * sizeof(rotated_expected);
    build_dynamic_record(&rotated_expected, "rotated-rkey-key");
    rotated_expected.version = 4;
    rotated_expected.checksum = record_checksum(&rotated_expected);
    printf("kv_rkey_rotate old=0x%x new=0x%x addr=0x%llx\n",
           old_remote_rkey, rotated_rkey,
           (unsigned long long)rotated_addr);

    if (rdma_cs_post_write_at(&context, &rotated_expected,
                              sizeof(rotated_expected),
                              remote_rotated_record_addr, rotated_rkey,
                              5901) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_rotated_rkey_write_cqe") != 0 ||
        rdma_cs_send_line(fd, "KV_ROTATED_RKEY_WRITE_DONE\n") != 0 ||
        expect_line(fd, line, sizeof(line),
                    "KV_ROTATED_RKEY_READ_READY\n") != 0 ||
        rdma_cs_post_read_at(&context, sizeof(rotated_expected),
                             remote_rotated_record_addr, rotated_rkey,
                             5902) != 0 ||
        rdma_cs_poll_success(&context, "kv_client_rotated_rkey_read_cqe") != 0 ||
        memcmp(context.buf, &rotated_expected, sizeof(rotated_expected)) != 0 ||
        rdma_cs_send_line(fd, "KV_ROTATED_RKEY_READ_DONE\n") != 0)
        goto out;
    puts("KV_RKEY_ROTATION_ACCESS_PASS");

    if (expect_line(fd, line, sizeof(line), "KV_OLD_RKEY_READY\n") != 0)
        goto out;
    /* 旧 rkey 的错误 CQE 可能令 QP 进入错误态，因此把它放在最后。 */
    if (rdma_cs_post_write_at(&context, &rotated_expected,
                              sizeof(rotated_expected),
                              remote_rotated_record_addr, old_remote_rkey,
                              5903) != 0 ||
        rdma_cs_poll_expect_error(&context,
                                  "kv_client_stale_rkey_write_cqe") != 0 ||
        rdma_cs_send_line(fd, "KV_OLD_RKEY_REJECTED\n") != 0)
        goto out;
    puts("KV_STALE_RKEY_REJECT_PASS");
    puts("KV_RKEY_BOUNDARY_PASS");
    puts("ONE_SIDED_KV_PASS");
    puts("ONE_SIDED_KV_CURRENT_ENV_COMPLETE");
    rc = EXIT_SUCCESS;

out:
    rdma_cs_close_fd(fd);
    rdma_cs_resources_destroy(&context);
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}

int main(int argc, char **argv)
{
    struct rdma_cs_options options;
    int parsed = rdma_cs_parse_common(argc, argv, &options,
                                      RDMA_CS_ROLE_CLIENT);

    if (parsed > 0)
        return EXIT_SUCCESS;
    if (parsed < 0)
        return EXIT_FAILURE;
    return run_full_client(&options);
}
