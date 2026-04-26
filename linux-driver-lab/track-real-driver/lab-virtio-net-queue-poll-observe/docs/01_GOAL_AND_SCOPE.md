# 01_GOAL_AND_SCOPE

## 目标

这个实验的目标不是“抓一堆 trace”，而是围绕 `virtio_net` 的 queue / poll 主线，回答下面几个问题：

1. RX 方向上的 event -> callback -> napi schedule -> poll 是否真的成立？
2. 在 idle / ping / iperf3 三种负载下，poll 节奏变化是什么？
3. refill / recycle 在运行期证据中能否间接看出来？
4. 当前的运行期观测，是否足以支撑下一步更细 patch / tracing？

## 当前做什么

- 选择少量关键观测点
- 跑三类 workload：idle / ping / iperf3
- 收集前后计数和 trace
- 写成一个 queue/poll 事件推进报告

## 当前不做什么

- 不改重主路径
- 不做太多 tracepoint 大杂烩
- 不要求一轮里同时验证 TX、RX、XDP、offload 全部细节
