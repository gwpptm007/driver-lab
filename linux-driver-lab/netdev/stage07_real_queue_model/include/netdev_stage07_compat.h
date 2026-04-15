/* SPDX-License-Identifier: GPL-2.0 */
/*
 * netdev_stage07_compat.h — stage07 内核兼容层
 *
 * 【设计目的】
 * 记录 stage07 代码中因内核版本差异而需要兼容处理的部分。
 * 所有版本判断集中在此文件，驱动主代码不直接出现 #if LINUX_VERSION_CODE。
 *
 * 【兼容内容】
 * 1. netif_napi_add 参数顺序变化
 *    - 5.x-6.7: netif_napi_add(ndev, napi, pollfn, weight)
 *    - 6.8+   : netif_napi_add_weight(ndev, napi, pollfn, weight)
 *      区别：6.8+ 把 weight 参数移到了最后，但语义相同
 *
 * 【使用方式】
 * 在驱动代码中统一使用 STAGE07_NETIF_NAPI_ADD 宏，
 * 编译时会根据当前内核版本自动选择正确版本。
 */

#ifndef _NETDEV_STAGE07_COMPAT_H_
#define _NETDEV_STAGE07_COMPAT_H_

#include <linux/version.h>
#include <linux/netdevice.h>

/*
 * STAGE07_NETIF_NAPI_ADD — NAPI 注册的版本兼容宏
 *
 * 【宏展开示例】
 * Linux 6.8+:
 *   netif_napi_add_weight((ndev), (napi), (pollfn), (weight))
 * Linux 5.x-6.7:
 *   netif_napi_add((ndev), (napi), (pollfn), (weight))
 *
 * 【为什么需要兼容？】
 * 6.8 重新设计了 NAPI weight 语义，原来是 poll() 的 budget 参数，
 * 6.8+ 改为独立权重参数。内核 API 做了如下调整：
 * - 旧 API: netif_napi_add(ndev, napi, poll_fn, weight)
 * - 新 API: netif_napi_add_weight(ndev, napi, poll_fn, weight)
 * 新 API 语义更清晰：weight 就是控制每轮 poll 最多处理多少 work 的上限。
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#define STAGE07_NETIF_NAPI_ADD(ndev, napi, pollfn, weight) \
	netif_napi_add_weight((ndev), (napi), (pollfn), (weight))
#else
#define STAGE07_NETIF_NAPI_ADD(ndev, napi, pollfn, weight) \
	netif_napi_add((ndev), (napi), (pollfn), (weight))
#endif

#endif /* _NETDEV_STAGE07_COMPAT_H_ */
