/* SPDX-License-Identifier: BSD-3-Clause
 *
 * gateway_common.h - 共享常量和类型定义
 */

#ifndef GATEWAY_COMMON_H
#define GATEWAY_COMMON_H

#include <stdbool.h>
#include <stdint.h>

/* 最大规则数（命令行配置最多 4 条规则） */
#define GW_MAX_RULES      4
/* 最大端口数（支持 vdev + 物理网卡混合） */
#define GW_MAX_PORTS      64
/* 规则名称字符串长度 */
#define GW_RULE_NAME_LEN  32

#endif /* GATEWAY_COMMON_H */
