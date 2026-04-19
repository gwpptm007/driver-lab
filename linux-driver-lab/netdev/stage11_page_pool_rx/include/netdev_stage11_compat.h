/* SPDX-License-Identifier: GPL-2.0 */
/* vim: set ts=8 sw=8 tw=8: */
#ifndef NETDEV_STAGE11_COMPAT_H
#define NETDEV_STAGE11_COMPAT_H

#include <linux/version.h>

/*
 * netdev_stage11_compat.h — 内核版本兼容宏
 *
 * stage11 soft 使用与 stage10 相同的 compat 宏：
 *   STAGE11_NETIF_NAPI_ADD  — add NAPI to netdev
 *   STAGE11_NAPI_COMPLETE   — napi complete (budget spent)
 */

/* Kernel 6.8+ uses netif_napi_add_weight */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#define STAGE11_NETIF_NAPI_ADD(ndev, napi, poll_fn, weight) \
    netif_napi_add_weight(ndev, napi, poll_fn, weight)
#else
#define STAGE11_NETIF_NAPI_ADD(ndev, napi, poll_fn, weight) \
    netif_napi_add(ndev, napi, poll_fn, weight)
#endif

/* NAPI complete — kernel 5.19+ uses napi_complete_done */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
#define STAGE11_NAPI_COMPLETE(napi, work_done)    napi_complete_done(napi, work_done)
#else
#define STAGE11_NAPI_COMPLETE(napi, work_done)    napi_complete(napi)
#endif

#endif /* NETDEV_STAGE11_COMPAT_H */