/* SPDX-License-Identifier: GPL-2.0 */
/*
 * netdev_stage08_compat.h — stage08 内核兼容层
 *
 * 当前主要兼容点：
 * - Linux 6.8+ 上 NAPI add API 的变化
 */

#ifndef _NETDEV_STAGE08_COMPAT_H_
#define _NETDEV_STAGE08_COMPAT_H_

#include <linux/version.h>
#include <linux/netdevice.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#define STAGE08_NETIF_NAPI_ADD(ndev, napi, pollfn, weight) \
	 netif_napi_add_weight((ndev), (napi), (pollfn), (weight))
#else
#define STAGE08_NETIF_NAPI_ADD(ndev, napi, pollfn, weight) \
	 netif_napi_add((ndev), (napi), (pollfn), (weight))
#endif

#endif
