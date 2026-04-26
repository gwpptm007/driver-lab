# queue_poll_report

## 目标

这是 `lab-virtio-net-queue-poll-observe` 的总报告入口。

## 当前建议结构

1. 观察目标与假设
2. idle / ping / iperf3 三轮结果
3. queue/poll 事件链证据
4. 与 `source-dive` / `runtime-observe` 的映射
5. 当前不足
6. 下一步更细 tracing 或 patch 建议

## 一句话定位

这是把：
- Round2 的 TX/RX 主路径
- Round3 的 queue/NAPI/IRQ 事件推进模型

真正转成运行期证据的实验。
