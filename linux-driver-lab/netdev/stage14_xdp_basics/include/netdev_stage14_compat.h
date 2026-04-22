/* SPDX-License-Identifier: GPL-2.0 */
/* vim: set ts=8 sw=8 tw=8: */
#ifndef NETDEV_STAGE14_COMPAT_H
#define NETDEV_STAGE14_COMPAT_H

#include <linux/version.h>
#include <linux/gfp.h>

/*
 * netdev_stage14_compat.h — 内核版本兼容宏
 *
 * stage14 soft 使用 compat 宏：
 *   STAGE14_NETIF_NAPI_ADD  — add NAPI to netdev
 *   STAGE14_NAPI_COMPLETE   — napi complete (budget spent)
 *   stage14_page_pool_*     — page_pool 兼容层
 *
 *  说明：
 *    page_pool API 在大多数内核中虽然存在 (=y)，但头文件未导出给模块。
 *    因此统一使用 alloc_pages / put_page 模拟 page_pool 行为。
 */

/* Kernel 6.8+ uses netif_napi_add_weight */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#define STAGE14_NETIF_NAPI_ADD(ndev, napi, poll_fn, weight) \
    netif_napi_add_weight(ndev, napi, poll_fn, weight)
#else
#define STAGE14_NETIF_NAPI_ADD(ndev, napi, poll_fn, weight) \
    netif_napi_add(ndev, napi, poll_fn, weight)
#endif

/* NAPI complete — kernel 5.19+ uses napi_complete_done */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
#define STAGE14_NAPI_COMPLETE(napi, work_done)    napi_complete_done(napi, work_done)
#else
#define STAGE14_NAPI_COMPLETE(napi, work_done)    napi_complete(napi)
#endif

/*========================================================
 *     ethtool ringparam 兼容层
 *
 *  kernel 6.8+ 使用 4-arg 版本:
 *    get_ringparam(ndev, ringparam, kernel_ringparam, ext_ack)
 *  kernel 5.15 使用 2-arg 版本:
 *    get_ringparam(ndev, ringparam)
 *========================================================*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#include <linux/netlink.h>
#include <linux/ethtool.h>
#define STAGE14_ETHTOOL_RINGPARAM_ARGS   struct ethtool_ringparam *ringparam, \
                                         struct kernel_ethtool_ringparam *kernel_ringparam, \
                                         struct netlink_ext_ack *extack
#else
#define STAGE14_ETHTOOL_RINGPARAM_ARGS   struct ethtool_ringparam *ringparam
#endif

/*========================================================
 *     page_pool 兼容层
 *
 *  page_pool API 在大多数内核中虽然配置 (=y)，但头文件未导出给模块。
 *  因此使用 alloc_pages / put_page 模拟 page_pool 行为。
 *
 *  page_pool 语义回顾：
 *    - page_pool_dev_alloc_pages() ≈ alloc_pages(GFP_ATOMIC | __GFP_COMP)
 *    - page_pool_put_page(pool, page, ...) ≈ put_page(page)
 *    - page_pool_destroy(pool) ≈ kfree（fallback 空操作）
 *
 *  注意：
 *    compat 层不依赖 CONFIG_PAGE_POOL，因为即使配置了，内核头文件
 *    也不一定导出给模块使用（如 Ubuntu 6.8 kernel）。
 *========================================================*/

struct stage14_page_pool {
    atomic_t pages_state_held_cnt;
};

/* 分配一个 page（等效 page_pool_dev_alloc_pages） */
static inline struct page *stage14_page_pool_alloc(struct stage14_page_pool *pool)
{
    return alloc_pages(GFP_ATOMIC | __GFP_COMP, 0);
}

/* 归还一个 page（等效 page_pool_put_page） */
static inline void stage14_page_pool_put(struct stage14_page_pool *pool,
                                        struct page *page)
{
    put_page(page);
}

/* 回收 direct（等效 page_pool_recycle_direct，失败路径） */
static inline void stage14_page_pool_recycle(struct stage14_page_pool *pool,
                                            struct page *page)
{
    put_page(page);
}

#endif /* NETDEV_STAGE14_COMPAT_H */
