/* SPDX-License-Identifier: GPL-2.0 */
/* 【学习】netdev_stage09_compat.h — 内核版本兼容性宏
 *
 * 内核 6.8.0 改变了 NAPI 添加函数：
 * - 旧版本（< 6.8.0）：netif_napi_add(ndev, napi, pollfn, weight)
 * - 新版本（>= 6.8.0）：netif_napi_add_weight(ndev, napi, pollfn, weight)
 *
 * 兼容宏的写法：
 * #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
 *   使用新版本
 * #else
 *   使用旧版本
 * #endif
 *
 * 教学亮点：为什么不直接用新版本？
 * - stage09 可能需要在旧内核（如 5.15 LTS）上测试
 * - 兼容宏让同一份代码能编译在多个内核版本上
 * - KERNEL_VERSION(6,8,0) 生成版本号 0x060800，用于数值比较
 *
 * _NETDEV_STAGE09_COMPAT_H_：防止重复包含的 header guard
 */
#ifndef _NETDEV_STAGE09_COMPAT_H_
#define _NETDEV_STAGE09_COMPAT_H_

#include <linux/version.h>
#include <linux/netdevice.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#define STAGE09_NETIF_NAPI_ADD(ndev, napi, pollfn, weight) \
	netif_napi_add_weight((ndev), (napi), (pollfn), (weight))
#else
#define STAGE09_NETIF_NAPI_ADD(ndev, napi, pollfn, weight) \
	netif_napi_add((ndev), (napi), (pollfn), (weight))
#endif

#endif
