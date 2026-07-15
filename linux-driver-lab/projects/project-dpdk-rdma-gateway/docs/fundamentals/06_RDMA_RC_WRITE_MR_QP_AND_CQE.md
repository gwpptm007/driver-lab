# 06. RDMA RC WRITE、MR、QP 与 CQE

RDMA 后端把 staging slot 中稳定的字节送往远端已授权的内存。它不通过 TCP 承载 payload；TCP 在当前实验中只承担控制面协商和测试同步。

## 6.1 需要区分的对象

| 对象 | 作用 | 生命周期重点 |
| --- | --- | --- |
| device/context | 访问 RDMA 设备与 verbs 上下文 | 设备选择、关闭顺序 |
| PD（Protection Domain） | 把 MR、QP 放进同一保护域 | 不能跨错误 PD 使用资源 |
| MR（Memory Region） | 注册本地或远端可访问的虚拟地址范围 | 地址、长度、lkey/rkey、注销时机 |
| CQ（Completion Queue） | 收集已请求 completion 的 WR 结果 | 深度、轮询、溢出处理 |
| QP（Queue Pair） | 维护 RC 发送队列与连接状态 | RESET -> INIT -> RTR -> RTS |
| WR/WQE | 一次具体的 RDMA 操作描述 | `wr_id` 与本地请求完成关联 |

当前路径使用 RC QP 和 `RDMA_WRITE`。本地 send buffer 必须是已注册 MR 中的地址；远端地址和 rkey 来自受控的连接建立协商，远端 NIC 会按 rkey 与 MR 边界执行访问检查。

## 6.2 控制面与数据面

建立 RC 连接需要双方交换足以把 QP 迁移到 RTR/RTS 的信息，例如 QPN、PSN、GID/LID（取决于链路）以及远端 MR 的 base address、length、rkey。当前项目用 TCP 做这类交换；它不改变 `RDMA_WRITE` 的一侧数据路径：

```text
TCP control plane: 交换连接和远端 MR 元数据
                         |
                         v
RDMA data plane: local registered staging/send buffer --RDMA_WRITE--> remote MR
```

远端接收端不会因为一次 one-sided WRITE 自动获得 receive CQE，也不会自动执行业务逻辑。当前测试服务端借助 TCP 的 `WRITE_DONE` 做验证同步，这一信号只是测试协议，不应被误解为远端持久化或事务提交确认。

## 6.3 一次写入的生命周期

worker 对已出队的请求执行如下序列：

1. 使用 slot payload 编码 40 字节 wire header；
2. 在已注册的本地发送缓冲区中组织 header + payload；
3. 验证 payload 长度、远端 offset 和远端 MR 边界；
4. 以一个带可关联 `wr_id` 的 WR 调用 `ibv_post_send`；
5. 轮询 CQ，检查 `wc.status`、opcode、byte length（如适用）与 `wr_id`；
6. 仅在成功 completion 与正确 `(slot_id, generation)` 对应后，释放 slot。

`ibv_post_send` 的返回值只说明 WR 是否被本地 provider 接受；真正的异步执行结果在 CQE 中。二者都必须处理。

## 6.4 CQE 不是一个泛化的“成功”按钮

CQE 至少需要回答三个问题：哪一个 WR 完成、它的 status 是否为成功、这个完成是否仍属于当前 slot generation。出现非成功 status 时，不能照常调用 `complete`，也不能立刻复用发送缓冲区或 staging slot。

当前阶段每次写入采用简单的带 completion 路径，便于逐请求建立归属关系。未来批量发送与 selective signaling 可以减少 CQ 压力，但必须显式保存“某一个 CQE 覆盖哪些未 signaled WR”的边界，且所有被覆盖的 payload 在可证明完成前均不能释放。

## 6.5 资源销毁的逆序

安全退出遵守“先停止新工作，再处理未完成工作，最后释放承载内存和 verbs 对象”：

```text
停止 producer
  -> drain request ring
  -> 停止 post 新 WR
  -> poll / 处理 outstanding WR 的成功、失败或 flush
  -> 销毁 QP/CQ
  -> 注销 MR
  -> 释放 PD/context
```

在仍可能 DMA 访问本地 MR 时注销 MR，或在仍可能收到 CQE 时释放完成映射，都会把可诊断的传输失败变成内存安全问题。

## 6.6 当前实验的结论边界

RXE/Soft-RoCE 和 pcap PMD 已证明：RC QP 可建立、远端 MR 可被写入、CQE 可驱动 slot 回收、端到端计数可闭合。它们不证明物理 RNIC 的吞吐、NUMA 拓扑、offload 行为、拥塞控制或真实网卡故障恢复。

权威接口语义可查阅 [ibv_post_send(3)](https://man7.org/linux/man-pages/man3/ibv_post_send.3.html) 和 [NVIDIA RDMA Aware Networks Programming User Manual](https://docs.nvidia.com/rdma-aware-networks-programming-user-manual-1-7.pdf)。项目实现位于 [`gateway_rdma_backend.c`](../../src/gateway_rdma_backend.c)。
