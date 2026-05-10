# 01_GOAL_AND_SCOPE

## 目标

当前 lab 的目标是理解并验证 AF_XDP 用户态收包模型。

重点不是性能，而是把这些对象串起来：

```text
XDP program
XSKMAP
AF_XDP socket
UMEM
FILL ring
RX ring
TX ring
COMPLETION ring
```

## 不做什么

本阶段暂不做：

- 多队列并发；
- zero-copy 性能对比；
- L2 forwarding；
- redirect 到另一个 AF_XDP socket；
- busy-poll/need-wakeup 调优。

这些放到后续 lab/project。
