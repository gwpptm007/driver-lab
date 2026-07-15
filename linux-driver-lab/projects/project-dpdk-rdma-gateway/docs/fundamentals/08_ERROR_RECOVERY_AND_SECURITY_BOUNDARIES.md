# 08. 错误恢复与安全边界

这个项目的首要错误处理目标不是“尽量继续跑”，而是避免把未知完成、未知内存或未知远端状态伪装为成功。特别是在 RDMA 中，错误地复用一个仍可能 DMA 的 slot，比丢弃一个包危险得多。

## 8.1 按层分类错误

| 层级 | 例子 | 当前应做什么 |
| --- | --- | --- |
| ingress 格式 | 截断包、非法 IHL、UDP length 矛盾 | 记 `malformed`、释放 mbuf、无 slot 泄漏 |
| ingress 策略 | ICMP、未支持 EtherType | 记 `unsupported`、不进入 staging |
| 本地容量 | ring 满、无 FREE slot | 记背压计数，取消 READY 或拒绝新请求 |
| verbs 投递 | `ibv_post_send` 立即失败 | 不标记成功，不释放未确定的资源 |
| 异步传输 | CQE status 非成功、QP 进入 error | 隔离连接，处理 outstanding/flush |
| 完成归属 | slot 越界、generation 不匹配 | 拒绝完成，保留当前 slot，升级告警 |
| 控制面 | TCP 协商断开、MR 元数据无效 | 当前连接不可用，禁止继续使用其 rkey/address |

分类的价值在于回答“谁可以安全继续”。例如 `unsupported` 不影响健康 QP，而 QP error 不能靠忽略一个计数器恢复。

## 8.2 generation mismatch 必须 fail closed

若一个 CQE 指向 slot 12、generation 3，但 slot 12 当前已是 generation 4，唯一安全结论是：该 CQE 不属于当前对象。实现不得因为“slot_id 相同”释放它，也不应以猜测方式回滚。应保留当前 slot、记录两代 generation 和相关 WR 信息，并让连接/worker 的上层故障策略决定下一步。

这条规则会损失短期可用性，却保护 payload 所有权，是 SPSC ring 之外第二道关键完整性防线。

## 8.3 RDMA 写入的授权范围

RDMA WRITE 不是任意远端内存写。发送端必须把远端 base address、length、rkey 当作不可信配置输入来验证：

```text
remote_offset + wire_length 不溢出
remote_offset + wire_length <= negotiated_remote_mr_length
wire_length == header_length + declared_payload_length
declared_payload_length <= GATEWAY_PAYLOAD_SIZE
```

rkey 由 RNIC 参与校验，但应用仍要做自己的长度与状态校验；不要让一个错误 offset 在本地通过而仅依赖硬件拒绝。控制面也不应把 rkey、地址、PSN 等敏感连接元数据写入逐包日志。

## 8.4 重试不是一个局部补丁

当前项目没有宣称 at-least-once 或 exactly-once 语义。一次 RDMA WRITE 在本地超时或 CQE 异常时，远端可能未写、已写、或状态未知。直接重发会产生重复副作用。

要支持重试，至少需要：

- 端到端 request ID，并由远端保存去重/幂等语义；
- 未确认 payload 的稳定存储和明确的 slot 保留规则；
- 连接重建后重新协商 QP 与远端 MR；
- 对“远端已写但本地未确认”的可恢复判定；
- 错误预算、重试上限、退避和最终失败队列。

这些是协议版本升级，不应隐藏在 `ibv_post_send` 外层的循环中。

## 8.5 最小安全日志与取证字段

错误事件应结构化记录：阶段、request ID、slot ID、generation、payload length、ingress port/queue、QP 标识、verbs errno 或 `wc.status`、时间戳。记录摘要而非完整 payload，避免日志泄露业务数据并防止错误风暴压垮数据面。

高频成功路径用计数器和采样，不逐包打印。错误路径也应有速率限制与聚合，例如同一 QP 的首次错误、状态变化和最后 N 个上下文。

## 8.6 当前与未来边界

Phase 4 已覆盖安全资源清理和实验性端到端错误可见性；尚未覆盖认证的控制面、租户隔离、远端访问授权轮换、自动 reconnect、持久化确认与生产故障域切换。把这些列为明确缺口，比把 RXE 环境当成生产安全证明更可靠。

相关设计：[`ARCHITECTURE.md`](../ARCHITECTURE.md)、[`gateway_rdma_backend.h`](../../include/gateway_rdma_backend.h)。
