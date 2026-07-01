#define _POSIX_C_SOURCE 200112L

#include "mr_lab.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mr_case {
    const char *name;
    int access;
    size_t offset;
    int expect_success;
};

static int run_case(struct mr_environment *env, const struct mr_case *test)
{
    void *allocation = NULL;
    unsigned char *address;
    struct ibv_mr *mr;
    size_t length = MR_TEST_SIZE - test->offset;
    int alloc_rc;
    int saved_errno;
    int actual_success;
    int pass;

    /* 保留原始 allocation，非对齐实验只改变交给 ibv_reg_mr() 的起始地址。 */
    alloc_rc = posix_memalign(&allocation, 4096, MR_TEST_SIZE);
    if (alloc_rc != 0) {
        printf("case=%s result=fail stage=allocation errno=%d\n", test->name, alloc_rc);
        return -1;
    }
    memset(allocation, 0x5a, MR_TEST_SIZE);
    address = (unsigned char *)allocation + test->offset;

    errno = 0;
    mr = ibv_reg_mr(env->pd, address, length, test->access);
    saved_errno = errno;
    actual_success = mr != NULL;
    pass = actual_success == test->expect_success;

    printf("case=%s access=0x%x offset=%zu aligned_4k=%s expected=%s actual=%s",
           test->name, test->access, test->offset,
           ((uintptr_t)address % 4096U) == 0 ? "yes" : "no",
           test->expect_success ? "success" : "failure",
           actual_success ? "success" : "failure");
    if (mr != NULL) {
        printf(" address=%p length=%zu lkey=0x%x rkey=0x%x",
               mr->addr, mr->length, mr->lkey, mr->rkey);
    } else {
        printf(" errno=%d message=%s", saved_errno, strerror(saved_errno));
    }
    printf(" result=%s\n", pass ? "pass" : "fail");

    /* 注销 MR 后 key 失效；必须在释放其覆盖的用户内存之前完成。 */
    if (mr != NULL)
        ibv_dereg_mr(mr);
    free(allocation);
    return pass ? 0 : -1;
}

int mr_run_suite(struct mr_environment *env)
{
    static const struct mr_case cases[] = {
        {"local_write", IBV_ACCESS_LOCAL_WRITE, 0, 1},
        {"remote_read_only", IBV_ACCESS_REMOTE_READ, 0, 1},
        {"remote_write_without_local_write", IBV_ACCESS_REMOTE_WRITE, 0, 0},
        {"all_permissions", IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                            IBV_ACCESS_REMOTE_WRITE, 0, 1},
        {"unaligned_all_permissions", IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                                      IBV_ACCESS_REMOTE_WRITE, 1, 1},
    };
    size_t i;
    int failures = 0;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (run_case(env, &cases[i]) != 0)
            ++failures;
    }
    printf("suite_cases=%zu suite_failures=%d suite_result=%s\n",
           sizeof(cases) / sizeof(cases[0]), failures,
           failures == 0 ? "pass" : "fail");
    return failures == 0 ? 0 : -1;
}
