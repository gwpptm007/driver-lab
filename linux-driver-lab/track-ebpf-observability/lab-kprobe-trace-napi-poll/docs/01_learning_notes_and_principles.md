# 学习笔记与原理

## 这一站学什么

这站不是学“怎么写一个很复杂的 bpftrace 工具”，而是学会用最小探针回答内核网络收包路径里的几个关键问题：

```text
硬中断到来
  -> 驱动关闭/抑制一部分中断
  -> 调度 NET_RX softirq
  -> softirq 上下文执行 NAPI poll
  -> 驱动 poll 函数批量收包
  -> GRO / skb / 协议栈继续处理
```

本 lab 关注中间这段：

```text
NET_RX softirq -> NAPI poll -> driver/helper poll
```

## NAPI 为什么存在

传统网卡每收一个包都触发中断，流量一大，CPU 会被中断打爆。NAPI 的思路是：

```text
低流量: 中断唤醒，响应快
高流量: 进入 poll，批量处理，减少中断风暴
```

所以 NAPI 不是“不要中断”，而是把中断和轮询结合起来。中断负责通知“有活了”，poll 负责“批量干活”。

## softirq 和 NAPI 的关系

Linux 网络 RX 路径通常不会在硬中断里做完整收包处理，而是把工作推迟到软中断。`NET_RX` softirq 是网络接收侧的重要入口。

本 lab 用：

```text
tracepoint:irq:softirq_entry
tracepoint:irq:softirq_exit
```

观察 `NET_RX` softirq 是否发生；再用 kprobe/kretprobe 观察 NAPI poll 是否执行。

## 为什么不是固定观测 napi_poll

不同内核配置、编译优化和符号属性会影响可观测符号。实际测试中，`napi_poll` 可能出现：

```text
napi_poll is not traceable
cannot attach kprobe
RC=255
```

这不代表 NAPI 路径不存在，只代表这个符号在当前内核上不适合作为 kprobe 入口。

因此当前脚本按顺序选择：

```text
napi_poll
__napi_poll
poll_one_napi
napi_threaded_poll
```

测试机最终选择的是：

```text
kprobe:__napi_poll
kretprobe:__napi_poll
```

## kprobe 和 kretprobe 分别看什么

`kprobe` 看函数进入：

```text
这个函数有没有被调用？
在哪个 CPU 上调用？
由哪个 comm 触发？
```

`kretprobe` 看函数返回：

```text
返回值分布是什么？
不同 CPU 上返回值是否不同？
```

在本轮测试中，`__napi_poll` 的返回值观测到了 `0` 和 `1`，说明 retprobe 成功拿到了返回路径证据。

## 这个 lab 的核心判断

一个有效的 NAPI 观测结果至少应该说明：

```text
1. 环境可用：bpftrace、ip、ethtool、timeout 存在。
2. 内核有可观测的 NAPI 或 poll 候选符号。
3. kprobe/kretprobe 不是 attach 失败。
4. NET_RX softirq 有事件。
5. NAPI poll 或 driver/helper probe 有正计数。
```

如果某个驱动 poll 函数没有计数，不一定失败。驱动符号、流量方向、虚拟网卡实现都会影响它。核心证据还是 softirq 与 NAPI poll 的关联。
