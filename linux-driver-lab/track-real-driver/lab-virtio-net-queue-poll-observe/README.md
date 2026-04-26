# lab-virtio-net-queue-poll-observe

> 所属：`track-real-driver/`

## 一句话定位

这是 `lab-virtio-net-source-dive/` 与 `lab-virtio-net-runtime-observe/` 之后的下一个并行实验：

> **把 `virtio_net` 中 queue / callback / napi schedule / poll / refill-recycle 这条“事件推进模型”，变成可观测、可记录、可复盘的实验。**

## 为什么现在做这个

当前你的推进状态是：

- `source-dive`：已经收口，理论基线已具备
- `runtime-observe`：已经有真实 baseline / ping 记录
- `ethtool-stats-mini-patch`：以后再测，不阻塞当前继续推进

所以现在最自然的下一步不是开更重主路径 patch，而是先把：

- queue
- callback
- napi schedule
- poll
- refill/recycle

这些点做成一条 **运行期事件链证据**。

## 这个 Lab 的核心目标

1. 确认 RX 侧 queue event -> napi schedule -> poll 的顺序证据
2. 记录不同 workload 下 poll 节奏变化
3. 把 queue/poll 观测结果和 `source-dive` 的 Round2/Round3 映射起来
4. 为后续更细 tracing 或 queue/poll 观测增强 patch 做准备

## 不追求的事

当前这一步不追求：

- 改重的 TX/RX 主路径语义
- 大范围 feature/offload/XDP 行为修改
- 一上来就加很多 patch
- 一次观察所有函数和所有 tracepoint

## 推荐阅读顺序

1. `START_HERE.md`
2. `docs/01_GOAL_AND_SCOPE.md`
3. `docs/02_OBSERVE_CHAIN.md`
4. `docs/03_TRACE_PLAN.md`
5. `docs/04_WORKLOAD_AND_METRICS.md`
6. `docs/05_EXECUTION_FLOW.md`
7. `docs/06_ACCEPTANCE_AND_REVIEW.md`
