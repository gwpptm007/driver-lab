/* SPDX-License-Identifier: GPL-2.0 */
/* vim: set ts=8 sw=8 tw=8: */
#ifndef NETDEV_STAGE10_COMPAT_H
#define NETDEV_STAGE10_COMPAT_H

#include <linux/version.h>

/*
 * netdev_stage10_compat.h —内核版本兼容宏
 *
 * stage09 uses kernel 6.8+ where netif_napi_add() requires weight parameter.
 * stage10 adds PCI/MSI-X which may run on different kernel versions.
 *
 * compat macros:
 *   STAGE10_NETIF_NAPI_ADD  — add NAPI to netdev
 *   STAGE10_NAPI_COMPLETE  — napi complete (budget spent)
 */

/* Kernel 6.8+ (confirmed in stage09 on 6.8.0-107-generic) uses netif_napi_add_weight */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#define STAGE10_NETIF_NAPI_ADD(ndev, napi, poll_fn, weight) \
    netif_napi_add_weight(ndev, napi, poll_fn, weight)
#else
#define STAGE10_NETIF_NAPI_ADD(ndev, napi, poll_fn, weight) \
    netif_napi_add(ndev, napi, poll_fn, weight)
#endif

/* NAPI complete — kernel 5.19+ uses napi_complete_done */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
#define STAGE10_NAPI_COMPLETE(napi, work_done)    napi_complete_done(napi, work_done)
#else
#define STAGE10_NAPI_COMPLETE(napi, work_done)    napi_complete(napi)
#endif

/* MSI-X status: standard in Linux >= 2.6.33 */
#define STAGE10_HAVE_MSIX

#endif /* NETDEV_STAGE10_COMPAT_H */
