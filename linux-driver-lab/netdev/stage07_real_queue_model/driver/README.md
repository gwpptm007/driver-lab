# driver

本目录放 stage07 的驱动主代码。

## 当前状态

这里已经不是纯 starter scaffold，而是第一版真实落地代码：

- `netdev_stage07.c` 已经实现单队列 queue model v1
- 重点不是“功能很多”，而是把下面几组关系真正落清楚：
  - TX submit / notify / complete
  - RX post / device write / consume / refill
  - irq / napi / completion

## 当前实现边界

- backend 仍是教学型 `memcpy`
- 单 TX queue + 单 RX queue
- 单 NAPI
- 有 debugfs 统计和队列 dump

## 继续推进时优先关注

1. smoke 是否稳定闭环
2. debugfs 统计是否和预期一致
3. queue index 是否始终单调轮转
4. stage04 和 stage07 的差异是否能被文档和代码同时解释清楚
