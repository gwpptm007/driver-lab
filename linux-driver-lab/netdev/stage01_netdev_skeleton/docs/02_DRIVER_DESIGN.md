# 02. 驱动设计说明

## 一、驱动模型

本阶段驱动是一个**教学型虚拟 net_device**：

- 通过 `alloc_etherdev_mqs()` 分配
- 通过 `register_netdev()` 注册到网络栈
- 通过 `ndo_open()` / `ndo_stop()` 维护最小生命周期
- 通过 `ndo_start_xmit()` 验证“用户态发包确实走到驱动”

## 二、为什么 `ndo_start_xmit()` 里直接消费 skb

因为 `stage01` 的重点不是“真正发到某个外部设备”，而是：

- 证明发包路径已经建立
- 把 `skb` 到驱动入口的链条先打通
- 建立最小统计与 debugfs 输出

因此 `ndo_start_xmit()` 当前采用：

1. 读取 `skb->len` 与 `skb->protocol`
2. 更新私有统计项
3. 直接 `dev_consume_skb_any()` 回收

这是一种教学策略，不是生产网卡写法。

## 三、为什么先做 debugfs

因为 `ip -s link` 只能看到标准统计项。教学阶段为了更容易解释，需要补充：

- `open_count`
- `stop_count`
- `last_len`
- `last_proto`

所以导出：

- `/sys/kernel/debug/netdev_stage01/stats`

这会成为后续阶段扩展 stats/debugfs 的起点。
