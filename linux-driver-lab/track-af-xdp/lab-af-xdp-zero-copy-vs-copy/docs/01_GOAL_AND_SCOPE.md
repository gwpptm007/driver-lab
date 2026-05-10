# 01_GOAL_AND_SCOPE

## 范围

本实验验证 AF_XDP 的 copy mode 和 zero-copy mode 边界：

- `skb + copy` 是否能作为基线启动；
- `native + copy` 是否受驱动支持；
- `native + zero-copy` 是否受驱动支持；
- zero-copy 失败时如何记录和 fallback。

## 不做

- 不做吞吐性能压测；
- 不做多队列 RSS；
- 不做完整用户态转发器；
- 不把 zero-copy 失败当成实验失败。
