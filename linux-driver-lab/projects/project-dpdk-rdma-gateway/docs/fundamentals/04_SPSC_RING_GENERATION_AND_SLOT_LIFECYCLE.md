# 04. SPSC Ring、generation 与 slot 生命周期

本项目的本地并发模型刻意很小：一个 DPDK ingress producer，把请求交给一个 RDMA worker consumer。它不是“所有线程都能拿”的通用队列，而是一个带明确所有权的 SPSC（single producer, single consumer）通道。

当前环大小为 `GATEWAY_RING_SIZE = 64`，每个元素是 32 字节 `gateway_request`。环中不放 payload，也不放 `rte_mbuf *`；它只把一个已经准备好的 staging slot 的身份交给消费者。

## 4.1 两个单调计数器

环维护 producer 与 consumer 两个单调递增的逻辑计数器。实现可以让它们自然回绕，但判断必须基于无符号差值，而不是“数组下标大小”。

```text
producer owns: 写 descriptor，推进 producer
consumer owns: 读 descriptor，推进 consumer

index = logical_counter % GATEWAY_RING_SIZE
used  = producer - consumer
empty: used == 0
full : used == GATEWAY_RING_SIZE
```

producer 只能修改 producer index，consumer 只能修改 consumer index。这一条比“使用了 atomic”更重要：一旦有第二个 producer 或 consumer 直接碰这两个索引，SPSC 的正确性证明就不再成立。

## 4.2 发布与获取：descriptor 何时可见

producer 的正确顺序是：

1. 填完 `gateway_request` 的每个字段；
2. 以 release 语义发布 producer index；
3. consumer 以 acquire 语义观察到新的 producer index 后，才读取 descriptor。

consumer 回收空间时同理：先完成 descriptor 的读取与处理，再以 release 语义推进 consumer index；producer 以 acquire 语义看到该进展后，才能覆盖相应位置。

这不是为了“让 CPU 更慢”，而是防止另一侧看到半写入 descriptor。例如 `slot_id` 已经新、`generation` 仍是旧值，会把正确的 CQE 防护变成偶发的数据破坏。

## 4.3 ring 与 slot 是两套状态

ring 的“已入队/已出队”不等于 payload 已经可复用。请求还对应 staging slot 的独立状态机：

```text
FREE --prepare_next--> READY --mark_inflight--> INFLIGHT --complete--> FREE
                         |                         |
                         | cancel_ready            | stale / failed CQE
                         v                         v
                        FREE                  保持原状态并上报错误
```

| 状态 | 所有者与允许动作 | 不允许的动作 |
| --- | --- | --- |
| `FREE` | producer 可选择、写 payload、递增 generation | worker 发送；producer 重用前一次 generation |
| `READY` | payload 完整，ring 中应有对应 descriptor | 再次 prepare；在尚未出队时当作 FREE |
| `INFLIGHT` | worker 已将其关联到未完成 RDMA 操作 | 覆盖 payload；因 stop 立即释放 |
| 完成匹配 | worker 用 `(slot_id, generation)` 完成并释放 | 只凭 `slot_id` 释放 |

`generation` 是 slot 重用次数。slot 7 的旧完成即使在 slot 7 已被分配给新请求后才到达，也不会因为编号相同而误释放新 payload。

## 4.4 正常、拥塞与异常路径

**正常路径**：producer 选择 FREE slot，`prepare_next` 写入 payload 与新 generation，构造 descriptor 并入队；worker 出队后 `mark_inflight`，RDMA 完成后以相同 generation 调用 `complete`。

**ring 满**：payload 已经被准备但 descriptor 尚未发布时，必须调用 `cancel_ready` 回退为 FREE；不可让一个没有 ring 引用的 READY slot 永久滞留。

**slot 耗尽**：所有 slot 都是 READY 或 INFLIGHT 时，producer 没有可写入空间，应记 `slot_exhausted` 并按当前策略丢弃/施压；绝不能覆盖正在飞行的 slot。

**陈旧 CQE**：`slot_id` 有效但 generation 不同，是完整性告警。实现应拒绝本次完成并保留当前 slot 状态，直到与当前 generation 匹配的结果或上层故障处理到来。

## 4.5 64 并不是性能调优参数

64 同时约束三件事：ring 中最多的待处理 descriptor 数、staging 池最多的活跃请求数、关闭时最多需要 drain 的工作量。改变它之前，需要重新确认：

- ring 容量与 slot 池容量是否仍然有意保持相同；
- 慢 RDMA 端能否在池耗尽时给 ingress 可观测的背压；
- 单批可投递的 WR 数、CQ 深度和停止 drain 时间是否匹配；
- 测试是否覆盖满、空、回绕、取消 READY、陈旧完成。

## 4.6 何时可以替换为 `rte_ring`

`rte_ring` 可以减少自建环的维护成本，但不是无条件“升级”。只有先重新写清楚并发模型时才应替换：

| 目标模型 | 需要明确的选择 |
| --- | --- |
| 仍为单 ingress + 单 worker | SP/SC fast path，slot 生命周期保持不变 |
| 多 RX queue | 每 queue 一个 SPSC ring，或选用 MP producer 语义并重新定义 slot 分配 |
| 多 RDMA worker | 每 worker 独立 QP/CQ 与 slot 分区，或使用 MC dequeue 并重新证明完成所有权 |

无论队列实现如何变化，`generation` 和“未完成 RDMA 前不得重用 payload”的规则都必须保留。队列只传递所有权，不替代端到端的完成语义。

## 4.7 最小回归清单

- 空环 dequeue 不读未初始化 descriptor；
- 满环 enqueue 不覆盖未消费元素，并使 READY slot 回退；
- 多次回绕后 FIFO 顺序保持；
- consumer 看到 descriptor 时字段完整；
- 旧 `(slot_id, generation)` 完成不能释放新一代 slot；
- stop 后 ring 被 drain，所有活动 slot 最终为 FREE 或被明确标记为失败。

相关代码：[`gateway_contract.h`](../../include/gateway_contract.h)、[`gateway_contract.c`](../../src/gateway_contract.c)、[`test_gateway_contract.c`](../../tests/test_gateway_contract.c)。
