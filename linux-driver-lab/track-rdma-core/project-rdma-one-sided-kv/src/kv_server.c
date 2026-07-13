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

    for (index = 0; index < RDMA_KV_KEY_SIZE; ++index)
        hash = (hash ^ key[index]) * 16777619U;
    for (index = 0; index < RDMA_KV_VALUE_SIZE; ++index)
        hash = (hash ^ value[index]) * 16777619U;
    return (hash ^ record->version) * 16777619U;
}

static void init_records(struct rdma_kv_record *records)
{
    unsigned int slot;

    /* server 注册整块数组；client 只能凭 rkey 和偏移访问目标槽位。 */
    memset(records, 0, sizeof(*records) * RDMA_KV_SLOT_COUNT);
    for (slot = 0; slot < RDMA_KV_SLOT_COUNT; ++slot) {
        snprintf(records[slot].key, sizeof(records[slot].key),
                 "server-key-%u", slot);
        snprintf(records[slot].value, sizeof(records[slot].value),
                 "server-initial-value-%u", slot);
        records[slot].version = 0;
        records[slot].checksum = record_checksum(&records[slot]);
    }
}

static int record_is_expected(const struct rdma_kv_record *record)
{
    struct rdma_kv_record expected;

    memset(&expected, 0, sizeof(expected));
    snprintf(expected.key, sizeof(expected.key), "client-key-%u",
             RDMA_KV_TEST_SLOT);
    snprintf(expected.value, sizeof(expected.value),
             "client-value-write-read-slot-%u", RDMA_KV_TEST_SLOT);
    expected.version = 1;
    expected.checksum = record_checksum(&expected);
    return memcmp(record, &expected, sizeof(expected)) == 0;
}

static int batch_records_are_expected(const struct rdma_kv_record *records)
{
    unsigned int index;

    for (index = 0; index < RDMA_KV_BATCH_CREDIT; ++index) {
        struct rdma_kv_record expected;
        unsigned int slot = RDMA_KV_BATCH_FIRST_SLOT + index;

        memset(&expected, 0, sizeof(expected));
        snprintf(expected.key, sizeof(expected.key), "client-credit-key-%u",
                 slot);
        snprintf(expected.value, sizeof(expected.value),
                 "client-credit-value-slot-%u", slot);
        expected.version = 2;
        expected.checksum = record_checksum(&expected);
        if (memcmp(&records[slot], &expected, sizeof(expected)) != 0 ||
            records[slot].checksum != record_checksum(&records[slot]))
            return 0;
    }
    return 1;
}

static int expect_line(int fd, char *line, size_t line_size,
                       const char *expected)
{
    return rdma_cs_recv_line(fd, line, line_size) == 0 &&
           strcmp(line, expected) == 0 ? 0 : -1;
}

static int run_full_server(const struct rdma_cs_options *options)
{
    struct rdma_cs_context context;
    struct rdma_cs_metadata local;
    struct rdma_cs_metadata remote;
    struct rdma_kv_record *records = NULL;
    struct rdma_kv_directory_entry *directory = NULL;
    uint64_t *remote_credit = NULL;
    uint32_t old_rkey;
    char line[RDMA_CS_LINE_SIZE];
    int listen_fd = -1;
    int conn_fd = -1;
    int rc = EXIT_FAILURE;

    rdma_cs_options_print(RDMA_CS_ROLE_SERVER, "one-sided-kv", options);
    rdma_cs_log_binding(RDMA_CS_ROLE_SERVER);
    if (RDMA_KV_DIRECTORY_OFFSET +
        sizeof(struct rdma_kv_directory_entry) *
            RDMA_KV_DIRECTORY_ENTRY_COUNT >
        RDMA_KV_ATOMIC_CREDIT_OFFSET) {
        fputs("kv_layout_error=buffer_too_small\n", stderr);
        goto out;
    }

    if (rdma_cs_resources_create(&context, options, 0x4b565331U) != 0)
        goto out;
    records = (struct rdma_kv_record *)context.buf;
    init_records(records);
    directory = (struct rdma_kv_directory_entry *)(context.buf +
                                                   RDMA_KV_DIRECTORY_OFFSET);
    memset(directory, 0, sizeof(*directory) * RDMA_KV_DIRECTORY_ENTRY_COUNT);
    remote_credit = (uint64_t *)(context.buf + RDMA_KV_ATOMIC_CREDIT_OFFSET);
    *remote_credit = RDMA_KV_BATCH_CREDIT;
    if (rdma_cs_metadata_from_context(&local, &context,
                                      RDMA_CS_ROLE_SERVER) != 0)
        goto out;

    listen_fd = rdma_cs_tcp_listen(options->listen_addr, options->tcp_port);
    if (listen_fd < 0)
        goto out;
    printf("server_listen=%s:%s\n", options->listen_addr, options->tcp_port);
    conn_fd = rdma_cs_tcp_accept(listen_fd);
    if (conn_fd < 0 || rdma_cs_exchange_metadata(conn_fd, &local, &remote) != 0 ||
        rdma_cs_qp_to_rts(&context, &remote) != 0)
        goto out;
    printf("kv_server_layout slots=%u record_size=%zu base_addr=0x%llx rkey=0x%x\n",
           RDMA_KV_SLOT_COUNT, sizeof(*records),
           (unsigned long long)local.addr, local.rkey);
    printf("kv_atomic_credit addr=0x%llx initial=%llu\n",
           (unsigned long long)(local.addr + RDMA_KV_ATOMIC_CREDIT_OFFSET),
           (unsigned long long)*remote_credit);
    puts("KV_SERVER_READY");

    if (rdma_cs_send_line(conn_fd, "KV_WRITE_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line), "KV_WRITE_DONE\n") != 0)
        goto out;
    if (!record_is_expected(&records[RDMA_KV_TEST_SLOT]) ||
        records[RDMA_KV_TEST_SLOT].checksum !=
            record_checksum(&records[RDMA_KV_TEST_SLOT]))
        goto out;
    printf("kv_write_record slot=%u key=%s value=%s version=%u checksum=0x%x\n",
           RDMA_KV_TEST_SLOT, records[RDMA_KV_TEST_SLOT].key,
           records[RDMA_KV_TEST_SLOT].value,
           records[RDMA_KV_TEST_SLOT].version,
           records[RDMA_KV_TEST_SLOT].checksum);
    puts("KV_WRITE_PASS");

    /* READ 不在 server CQ 产生完成；TCP ACK 只说明 client 已完成本地校验。 */
    if (rdma_cs_send_line(conn_fd, "KV_READ_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line), "KV_READ_DONE\n") != 0)
        goto out;
    puts("KV_READ_PASS");

    /* TCP 公布额度，真正的占用由远端 fetch-and-add 原子减 4 完成。 */
    if (rdma_cs_send_line(conn_fd, "KV_CREDIT grant=4\n") != 0 ||
        rdma_cs_send_line(conn_fd, "KV_ATOMIC_CREDIT_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_ATOMIC_ACQUIRED old=4\n") != 0 ||
        *remote_credit != 0)
        goto out;
    puts("KV_REMOTE_ATOMIC_ACQUIRE_PASS");

    if (rdma_cs_send_line(conn_fd, "KV_BATCH_WRITE_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_BATCH_WRITE_DONE count=4\n") != 0)
        goto out;
    puts("KV_CREDIT_GRANTED count=4");
    puts("KV_CREDIT_PASS");
    if (!batch_records_are_expected(records))
        goto out;
    printf("kv_batch_write first_slot=%u count=%u tail_key=%s\n",
           RDMA_KV_BATCH_FIRST_SLOT, RDMA_KV_BATCH_CREDIT,
           records[RDMA_KV_BATCH_FIRST_SLOT + RDMA_KV_BATCH_CREDIT - 1].key);
    puts("KV_BATCH_WRITE_PASS");

    if (rdma_cs_send_line(conn_fd, "KV_BATCH_READ_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_BATCH_READ_DONE count=4\n") != 0)
        goto out;
    puts("KV_BATCH_READ_PASS");

    if (rdma_cs_send_line(conn_fd, "KV_ATOMIC_RETURN_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_ATOMIC_RETURNED old=0\n") != 0 ||
        *remote_credit != RDMA_KV_BATCH_CREDIT)
        goto out;
    printf("kv_atomic_credit_final=%llu\n",
           (unsigned long long)*remote_credit);
    puts("KV_REMOTE_ATOMIC_RETURN_PASS");
    puts("KV_REMOTE_ATOMIC_CREDIT_PASS");

    if (rdma_cs_send_line(conn_fd, "KV_CAS_HOLDER_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_CAS_HOLDER_ACQUIRED old=4\n") != 0 ||
        *remote_credit != 0)
        goto out;
    puts("KV_CAS_HOLDER_ACQUIRE_PASS");

    /* 竞争者 CAS 失败时 counter 必须仍为 0，不能破坏持有者的额度。 */
    if (rdma_cs_send_line(conn_fd, "KV_CAS_CONTENDER_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_CAS_CONTENDER_REJECTED old=0\n") != 0 ||
        *remote_credit != 0)
        goto out;
    puts("KV_CAS_CONTENDER_REJECT_PASS");

    if (rdma_cs_send_line(conn_fd, "KV_CAS_HOLDER_RETURN_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_CAS_HOLDER_RETURNED old=0\n") != 0 ||
        *remote_credit != RDMA_KV_BATCH_CREDIT)
        goto out;

    if (rdma_cs_send_line(conn_fd, "KV_CAS_RETRY_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_CAS_RETRY_ACQUIRED old=4\n") != 0 ||
        *remote_credit != 0)
        goto out;
    puts("KV_CAS_RETRY_ACQUIRE_PASS");

    if (rdma_cs_send_line(conn_fd, "KV_CAS_RETRY_RETURN_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_CAS_RETRY_RETURNED old=0\n") != 0 ||
        *remote_credit != RDMA_KV_BATCH_CREDIT)
        goto out;
    printf("kv_cas_credit_final=%llu\n",
           (unsigned long long)*remote_credit);
    puts("KV_CAS_CREDIT_RECOVERY_PASS");
    puts("KV_CAS_CONTENTION_PASS");

    if (rdma_cs_send_line(conn_fd, "KV_DIRECTORY_PUT_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_DIRECTORY_PUT_DONE\n") != 0)
        goto out;
    {
        uint32_t hash = rdma_kv_key_hash("dynamic-alpha");
        uint32_t bucket = hash % RDMA_KV_DIRECTORY_ENTRY_COUNT;

        if (directory[bucket].state != RDMA_KV_DIRECTORY_OCCUPIED ||
            directory[bucket].hash != hash ||
            directory[bucket].slot != bucket ||
            strcmp(directory[bucket].key, "dynamic-alpha") != 0 ||
            strcmp(records[bucket].key, "dynamic-alpha") != 0 ||
            records[bucket].checksum != record_checksum(&records[bucket]))
            goto out;
        printf("kv_directory_server key=%s hash=0x%x bucket=%u slot=%u\n",
               directory[bucket].key, hash, bucket, directory[bucket].slot);
    }
    puts("KV_DYNAMIC_KEY_PUT_PASS");

    if (rdma_cs_send_line(conn_fd, "KV_DIRECTORY_GET_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_DIRECTORY_GET_DONE\n") != 0)
        goto out;
    puts("KV_DYNAMIC_KEY_GET_PASS");

    if (rdma_cs_send_line(conn_fd, "KV_DIRECTORY_COLLISION_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_DIRECTORY_COLLISION_REJECTED\n") != 0)
        goto out;
    {
        uint32_t hash = rdma_kv_key_hash("dynamic-alpha");
        uint32_t bucket = hash % RDMA_KV_DIRECTORY_ENTRY_COUNT;

        /* 被拒绝的碰撞请求不能改写目录或原 value。 */
        if (strcmp(directory[bucket].key, "dynamic-alpha") != 0 ||
            strcmp(records[bucket].key, "dynamic-alpha") != 0 ||
            records[bucket].checksum != record_checksum(&records[bucket]))
            goto out;
    }
    puts("KV_DIRECTORY_COLLISION_PASS");
    puts("KV_DYNAMIC_DIRECTORY_PASS");

    if (rdma_cs_rotate_mr(&context, &old_rkey) != 0 ||
        context.mr->rkey == old_rkey)
        goto out;
    snprintf(line, sizeof(line), "KV_RKEY_ROTATED addr=0x%llx rkey=0x%x\n",
             (unsigned long long)(uintptr_t)context.buf, context.mr->rkey);
    printf("kv_rkey_rotate old=0x%x new=0x%x addr=0x%llx\n",
           old_rkey, context.mr->rkey,
           (unsigned long long)(uintptr_t)context.buf);
    if (rdma_cs_send_line(conn_fd, line) != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_ROTATED_RKEY_WRITE_DONE\n") != 0 ||
        strcmp(records[RDMA_KV_ROTATION_SLOT].key,
               "rotated-rkey-key") != 0 ||
        records[RDMA_KV_ROTATION_SLOT].checksum !=
            record_checksum(&records[RDMA_KV_ROTATION_SLOT]) ||
        rdma_cs_send_line(conn_fd, "KV_ROTATED_RKEY_READ_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_ROTATED_RKEY_READ_DONE\n") != 0)
        goto out;
    puts("KV_RKEY_ROTATION_ACCESS_PASS");

    if (rdma_cs_send_line(conn_fd, "KV_OLD_RKEY_READY\n") != 0 ||
        expect_line(conn_fd, line, sizeof(line),
                    "KV_OLD_RKEY_REJECTED\n") != 0)
        goto out;
    puts("KV_STALE_RKEY_REJECT_PASS");
    puts("KV_RKEY_BOUNDARY_PASS");
    puts("ONE_SIDED_KV_PASS");
    puts("ONE_SIDED_KV_CURRENT_ENV_COMPLETE");
    rc = EXIT_SUCCESS;

out:
    rdma_cs_close_fd(conn_fd);
    rdma_cs_close_fd(listen_fd);
    rdma_cs_resources_destroy(&context);
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}

int main(int argc, char **argv)
{
    struct rdma_cs_options options;
    int parsed = rdma_cs_parse_common(argc, argv, &options,
                                      RDMA_CS_ROLE_SERVER);

    if (parsed > 0)
        return EXIT_SUCCESS;
    if (parsed < 0)
        return EXIT_FAILURE;
    return run_full_server(&options);
}
