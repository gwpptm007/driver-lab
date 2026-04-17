// SPDX-License-Identifier: GPL-2.0
/*
 * etdev_stage08_compat.h — stage08 兼容性头文件
 *
 * 【学习要点】
 *
 * 1. 为什么需要 compat 头文件
 *    内核版本差异导致 API 变化，compat 头文件统一这些差异，
 *    让驱动代码在不同内核版本上都能编译通过。
 *
 * 2. 主要兼容项
 *    - NAPI 相关宏（不同内核版本命名不同）
 *    - debugfs_create_file 的参数差异
 *    - 其他内核 API 差异
 *
 * 3. 使用方式
 *    在驱动源码中 include 此头文件，使用统一宏/函数，
 *    而不直接使用内核原始 API。
 */

#ifndef NETDEV_STAGE08_COMPAT_H
#define NETDEV_STAGE08_COMPAT_H

#include <linux/version.h>

/*
 * NAPI 兼容层
 *
 * 【学习要点】NAPI 注册方式的变化
 *
 * 旧内核 ( <= 5.9):
 *   netif_napi_add(ndev, &priv->napi, poll, weight);
 *
 * 新内核 (>= 5.10):
 *   netif_napi_add_tx_weight(ndev, &priv->napi, poll, weight);
 *   或者使用 NETIF_F_NAPI_ADD_WEIGHT 风格的宏
 *
 * 兼容宏 STAGE08_NETIF_NAPI_ADD 统一这两种写法
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
    // 【学习】netif_napi_add_tx_weight 是 5.10+ 引入的，明确指定 TX weight
    #define STAGE08_NETIF_NAPI_ADD(ndev, napi, poll, weight) \
        netif_napi_add_tx_weight(ndev, napi, poll, weight)
#else
    // 【学习】旧版本直接用 netif_napi_add，weight 参数意义相同
    #define STAGE08_NETIF_NAPI_ADD(ndev, napi, poll, weight) \
        netif_napi_add(ndev, napi, poll, weight)
#endif

/*
 * debugfs 兼容层
 *
 * 【学习要点】debugfs_create_file 的 mode 参数历史变化
 *
 * 旧内核 ( <= 5,10):
 *   debugfs_create_file(name, mode, parent, data, fops);
 *   mode 是 int 类型
 *
 * 新内核 (>= 5,11):
 *   debugfs_create_file(name, flags, parent, data, fops);
 *   mode 变成 unsigned int，并且语义略有变化
 *
 * 兼容处理：统一使用 0444（所有用户可读）
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
    // 【学习】新版本使用 S_IRUGO 等标准 POSIX 权限标志
    #define STAGE08_DEBUGFS_FILE_MODE 0444
#else
    // 【学习】旧版本直接用八进制数字
    #define STAGE08_DEBUGFS_FILE_MODE 0444
#endif

#endif /* NETDEV_STAGE08_COMPAT_H */
