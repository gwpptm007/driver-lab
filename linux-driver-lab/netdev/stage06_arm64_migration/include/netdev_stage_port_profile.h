/* SPDX-License-Identifier: GPL-2.0 */
/*
 * netdev_stage_port_profile.h
 *
 * stage06 的第二个代码交付：为 host / x86_64 / arm64 提供建议默认值。
 *
 * 这里不是强制策略，而是：
 *   - 给 smoke / sample 配一个更清晰的默认值来源
 *   - 让平台迁移时，ring_size / napi_weight / rx_buf_size 的差异更容易解释
 */
#ifndef NETDEV_STAGE_PORT_PROFILE_H
#define NETDEV_STAGE_PORT_PROFILE_H

#if defined(__aarch64__)
#define NETDEV_STAGE_DEFAULT_RING_SIZE   64
#define NETDEV_STAGE_DEFAULT_NAPI_WEIGHT 16
#define NETDEV_STAGE_DEFAULT_RX_BUF_SIZE 2048
#define NETDEV_STAGE_DEFAULT_PROFILE     "arm64"
#elif defined(__x86_64__)
#define NETDEV_STAGE_DEFAULT_RING_SIZE   64
#define NETDEV_STAGE_DEFAULT_NAPI_WEIGHT 16
#define NETDEV_STAGE_DEFAULT_RX_BUF_SIZE 2048
#define NETDEV_STAGE_DEFAULT_PROFILE     "x86_64"
#else
#define NETDEV_STAGE_DEFAULT_RING_SIZE   32
#define NETDEV_STAGE_DEFAULT_NAPI_WEIGHT 8
#define NETDEV_STAGE_DEFAULT_RX_BUF_SIZE 1536
#define NETDEV_STAGE_DEFAULT_PROFILE     "generic"
#endif

#endif /* NETDEV_STAGE_PORT_PROFILE_H */
