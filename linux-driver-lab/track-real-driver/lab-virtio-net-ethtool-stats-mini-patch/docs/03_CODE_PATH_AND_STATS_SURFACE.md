# 03_CODE_PATH_AND_STATS_SURFACE

## 这一轮重点看什么

和 `lab-virtio-net-source-dive` 不同，这一轮不再追所有路径，  
而是只聚焦和 `ethtool / stats` 直接相关的代码面。

重点是三件事：

1. stats 数据从哪里来
2. ethtool 是如何把它们暴露出去的
3. 你选中的 patch 点，在这条链上的什么位置

## 建议建立的三张表

### 1. 统计项表
| 统计项 | 来源位置 | 暴露位置 | workload 下预期变化 |
|---|---|---|---|

### 2. 控制面表
| 接口/输出方式 | 用途 | 当前 patch 是否涉及 |
|---|---|---|

### 3. 验证表
| 验证动作 | before 看什么 | after 看什么 |
|---|---|---|

## 和前面两个 Lab 的关系

### 来自 `source-dive`
- 你已经知道 `ethtool` 属于控制面和能力暴露出口
- 你已经知道 stats 不属于主路径本体，但和主路径密切关联

### 来自 `runtime-observe`
- 你已经有一轮日志、stats、workload 证据
- 你现在要把这些证据转成 patch 的 before 基线
