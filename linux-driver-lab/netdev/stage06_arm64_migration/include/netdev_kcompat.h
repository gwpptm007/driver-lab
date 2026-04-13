/* SPDX-License-Identifier: GPL-2.0 */
/*
 * netdev_kcompat.h
 *
 * stage06_arm64_migration 的核心代码交付之一：
 * 把前面阶段里已经遇到过的内核版本差异，收敛到一个可复用头文件里。
 *
 * 目标不是覆盖所有内核版本，而是先把当前项目里最真实、最常见的差异点抽出来：
 *   1. netif_napi_add vs netif_napi_add_weight
 *   2. u64_stats 在较新内核上的 begin/fetch API 差异
 */
#ifndef NETDEV_KCOMPAT_H
#define NETDEV_KCOMPAT_H

#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/u64_stats_sync.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#define NETDEV_KCOMPAT_NAPI_ADD(ndev, napi, pollfn, weight) \
	netif_napi_add_weight((ndev), (napi), (pollfn), (weight))
#else
#define NETDEV_KCOMPAT_NAPI_ADD(ndev, napi, pollfn, weight) \
	netif_napi_add((ndev), (napi), (pollfn), (weight))
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
/* 6.8+: returns flags via return value */
#define NETDEV_KCOMPAT_U64_UPDATE_BEGIN(syncp, flags) \
	do { (flags) = u64_stats_update_begin_irqsave((syncp)); } while (0)
#define NETDEV_KCOMPAT_U64_UPDATE_END(syncp, flags) \
	do { u64_stats_update_end_irqrestore((syncp), (flags)); } while (0)
#define NETDEV_KCOMPAT_U64_FETCH_BEGIN(syncp, start) \
	do { (start) = u64_stats_fetch_begin((syncp)); } while (0)
#define NETDEV_KCOMPAT_U64_FETCH_RETRY(syncp, start) \
	u64_stats_fetch_retry((syncp), (start))
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
/* 5.15-6.7: 1-arg form, returns void */
#define NETDEV_KCOMPAT_U64_UPDATE_BEGIN(syncp, flags) \
	u64_stats_update_begin_irqsave((syncp))
#define NETDEV_KCOMPAT_U64_UPDATE_END(syncp, flags) \
	u64_stats_update_end_irqrestore((syncp), (flags))
#define NETDEV_KCOMPAT_U64_FETCH_BEGIN(syncp, start) \
	do { (start) = u64_stats_fetch_begin((syncp)); } while (0)
#define NETDEV_KCOMPAT_U64_FETCH_RETRY(syncp, start) \
	u64_stats_fetch_retry((syncp), (start))
#else
/* 5.14 and older: 2-arg form */
#define NETDEV_KCOMPAT_U64_UPDATE_BEGIN(syncp, flags) \
	u64_stats_update_begin_irqsave((syncp), (flags))
#define NETDEV_KCOMPAT_U64_UPDATE_END(syncp, flags) \
	u64_stats_update_end_irqrestore((syncp), (flags))
#define NETDEV_KCOMPAT_U64_FETCH_BEGIN(syncp, start) \
	do { (start) = u64_stats_fetch_begin_irq((syncp)); } while (0)
#define NETDEV_KCOMPAT_U64_FETCH_RETRY(syncp, start) \
	u64_stats_fetch_retry_irq((syncp), (start))
#endif

#endif /* NETDEV_KCOMPAT_H */
