# 01_STAGE_OVERVIEW — stage12 概述

## stage12 学习目标

掌握 Linux `ethtool_ops` 控制面接口，理解真实 NIC 驱动如何向用户空间暴露配置和统计能力：

1. **ethtool_ops 完整实现**：stats / ringparam / channels / priv_flags
2. **标准接口 vs debugfs**：ethtool 是 Linux 标准接口，debugfs 是辅助
3. **stats 导出机制**：`get_strings` / `get_sset_count` / `get_ethtool_stats`
4. **控制操作**：ringparam / channels 的查询和配置语义

## 什么是 ethtool

ethtool 是 Linux 标准网络设备配置工具，驱动通过 `ethtool_ops` 向用户空间暴露：

```
$ ethtool -i eth0        # 驱动信息
$ ethtool -S eth0        # 统计信息
$ ethtool -G eth0        # ring 参数
$ ethtool -L eth0        # 队列数
$ ethtool -a eth0        # pause 参数
```

真实驱动必须支持 ethtool，否则无法被标准工具管理。

## 与 stage11 的对比

| 维度 | stage11 | stage12 |
|------|---------|---------|
| 统计接口 | debugfs | ethtool + debugfs |
| 用户空间 | 专用工具 | 标准 ethtool |
| 控制操作 | 无 | ringparam / channels 查询 |
| 接口标准 | 非标准 | Linux 标准接口 |

## stage12 新增功能

```
ethtool -i nds12s        # 驱动信息
ethtool -S nds12s        # 标准统计导出
ethtool -G nds12s        # ring 参数查询
ethtool -L nds12s        # channel 数查询
ethtool --show-priv-flags nds12s  # 私有标志
```

## ethtool_ops 回调对应关系

| ethtool 命令 | 回调函数 |
|-------------|---------|
| `ethtool -i` | `get_drvinfo` |
| `ethtool -S` | `get_strings` + `get_sset_count` + `get_ethtool_stats` |
| `ethtool -G` | `get_ringparam` / `set_ringparam` |
| `ethtool -L` | `get_channels` / `set_channels` |
| `ethtool --show-priv-flags` | `get_priv_flags` / `set_priv_flags` |

## 真实驱动参考

- **igb**：Intel 千兆网卡，完整 ethtool_ops 实现
- **ixgbe**：Intel 万兆网卡，丰富的 stats 和 offload 支持
- **virtio-net**：半虚拟化驱动，基础 ethtool 支持
- 共同模式：所有标准 Linux NIC 驱动都实现 ethtool_ops