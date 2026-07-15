# 07. Worker、背压与优雅 drain

一个数据面项目的停止和拥塞路径，与正常路径同样重要。当前设计由 ingress 生产请求、一个 RDMA worker 消费请求；worker 是 QP、CQ、RDMA 发送缓冲区和 INFLIGHT slot 的唯一执行者。

## 7.1 worker 的职责边界

worker 的循环可概括为：

```text
dequeue (acquire)
  -> READY 变为 INFLIGHT
  -> 构造 wire bytes
  -> post RDMA WRITE
  -> poll CQ
  -> 用 completion 的 generation 完成 slot
```

producer 不应绕过该循环去碰 QP/CQ；同样，worker 不应反过来读 DPDK mbuf。这样每个外部资源都有单一线程所有者，出错时可以按 slot、request、WR 三个层面追踪。

## 7.2 背压链条

慢的远端最终会通过有限资源传回 ingress：

```text
远端 / 网络变慢
  -> CQE 变慢，INFLIGHT slot 停留更久
  -> FREE slot 减少
  -> producer 出现 slot_exhausted

worker 消费跟不上
  -> SPSC ring 接近满
  -> producer 出现 ring_full，并取消刚准备的 READY slot
```

`slot_exhausted` 和 `ring_full` 不是同一个信号：前者说明 payload 池被 READY/INFLIGHT 占用，后者说明本地 descriptor 交接带宽不足。监控和限流策略应分别对待。

## 7.3 当前模型的有意限制

当前代码采用小规模、易验证的发送/完成路径，不把“批量 WR”“多个 QP”“多个 worker”伪装成已经具备的能力。具体限制包括：

- 单 ingress producer、单 RDMA consumer；
- staging slot 数和 ring 深度均为 64；
- 完成归属以每个 slot 的 generation 为基础；
- 不承诺请求重试、远端幂等或生产级拥塞控制。

这些限制使 Phase 4 可以把 `staged = dequeued = completed` 作为强断言。扩展时首先要替换这些断言为适合多队列/批量模型的守恒关系，而不是悄悄放宽它们。

## 7.4 优雅停止的协议

停止信号只表示“不再接收/发布新请求”，不表示“可以丢弃已经发布的请求”。推荐顺序：

1. ingress 停止从 RX 获取新包，并结束当前 burst 的资源归还；
2. producer 不再向 ring 发布新 descriptor；
3. worker 继续 dequeue，直到 ring 为空；
4. worker 继续轮询，直到每个已 post 的 WR 都得到成功、失败或 flush 结果；
5. 根据结果完成、隔离或标记 slot，再销毁 verbs 资源。

因此 worker 的退出条件必须同时考虑“已请求 stop”和“ring 已空且无 outstanding WR”。只检测 stop flag 会留下 READY 或 INFLIGHT slot，且破坏测试中的 active-slot 归零断言。

## 7.5 失败时的 drain

发生 post-send、CQE 或 QP 错误时，不能继续把故障连接当作健康通道。安全做法是：停止向该 QP 投递；记录发生错误的 request/slot/generation；处理已有 completion 或 flush；将未确认完成的请求交给明确的失败/重试策略。当前阶段以“可见失败并安全清理”为目标，尚未实现自动 reconnect 或 at-least-once 投递。

如果将来加入重试，必须增加远端去重键和持久/幂等语义；仅在本地重新 post 同一个 payload 不能保证不会造成重复写入。

## 7.6 下一阶段的可验证扩展

Phase 5 的 batch 与 selective signaling 至少应新增：SQ budget、未完成 WR 数、CQ poll 批次、最大 ring occupancy、slot 高水位，以及基于压力的 ingress 降载策略。验收条件应包括“关闭时所有批次都有可解释的完成归属”，而不只是吞吐数字上升。

相关代码：[`gateway_rdma_worker.h`](../../include/gateway_rdma_worker.h)、[`gateway_rdma_worker.c`](../../src/gateway_rdma_worker.c)、[`test_gateway_end_to_end.c`](../../tests/test_gateway_end_to_end.c)。
