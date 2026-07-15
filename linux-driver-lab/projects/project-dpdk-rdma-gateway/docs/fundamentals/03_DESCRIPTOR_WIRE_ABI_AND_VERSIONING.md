# 03：Descriptor、wire ABI 与版本演进

## 两种格式解决两种问题

`gateway_request` 是本机 SPSC ring 内部消息；wire header 是 client/server 之间的协议。把两者拆开可以避免把线程调度细节、指针/slot 索引和编译器布局泄漏到远端。

| 格式 | 大小 | 字节序 | 可包含 | 不应包含 |
| --- | ---: | --- | --- | --- |
| local descriptor | 32 bytes | host order | `slot_id`、ingress port、RX queue、generation | 指针、远端 rkey、编译器 padding 依赖 |
| wire header | 40 bytes | explicit big-endian | magic、version、length、request id、flow hash、payload length、generation | `slot_id`、本机指针、未定义 padding |

## 固定本地 ABI

`_Static_assert(sizeof(struct gateway_request) == 32)` 把本机 ABI 漂移提前为编译错误。它不意味着可以把 struct 直接发送：字段对齐、padding、endianness 和未来编译器/架构变化都可能破坏远端兼容。

本地 descriptor 的字段还承担诊断作用：`ingress_port`、`rx_queue` 和 `flow_hash` 让后续扩展可以解释请求来自哪里、应如何分片或重放。

## wire decoder 的防御顺序

解码器应在使用 payload 前检查：

1. buffer 是否至少容纳 40-byte header；
2. magic 是否匹配；
3. version/header length 是否为本实现支持的组合；
4. reserved 字段是否为零；
5. opcode 是否受支持；
6. payload length 是否在 `1..GATEWAY_MAX_PAYLOAD`；
7. 整个 record 长度是否不越界。

这些规则使协议错误成为显式错误码，而不是远端内存解释错误。Phase 1 已覆盖 6 类负向 case；新增字段或 opcode 时必须增加对应 case。

## version 不是装饰字段

未来 v2 不能简单“在 header 末尾加字段，再假设 v1 decoder 会忽略”。兼容策略需要明确：

| 变化 | 推荐策略 |
| --- | --- |
| 新增可选能力 | 协商 capability，保留旧 version decoder |
| 改变字段语义/长度 | 新 version + 新 header length + 双 decoder/明确拒绝 |
| 增加 opcode | 旧 peer 明确返回 unsupported，不当成 WRITE |
| 改变 payload 语义 | 把内容协议单独版本化，不污染传输 header |

当前项目没有控制面 capability negotiation；因此本阶段正确的行为是严格拒绝不支持的 version，而不是“尽量猜测”。

## `request_id`、generation 与幂等性

- `request_id` 用于端到端关联、日志和远端验证；
- generation 用于本地 slot 防止 stale completion；
- 两者不是同一概念，不能互相代替；
- RC WRITE 的传输顺序不自动提供业务幂等性。重试、断链恢复或远端多 record 时，需要明确重复 write 如何处理。

## 远端 record 与应用确认

当前 remote record 是单一验证区域。server 通过 TCP `WRITE_DONE` 测试 token 在 client CQE 后读取/解码它。该 token 只服务测试同步；生产协议仍需定义 record ownership、commit marker、checksum、持久化与应用 ack。

## 扩展检查表

- 是否保留旧 decoder 的行为和测试向量？
- wire length 是否完整覆盖 header + payload？
- 新字段是否有明确 byte order 与 reserved 规则？
- request id 是否可关联到 ingress 和 CQE？
- 远端是否能拒绝未知 opcode/version 而不写错内存？
- 是否更新 golden test、negative test、文档和 test record？
