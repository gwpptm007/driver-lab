# 05：Virtqueue、共享内存、所有权与事件通知

## virtqueue 解决的核心问题

guest driver 与后端位于不同执行环境，不能直接共享普通内核指针。virtio 用协商出的共享内存队列传递 buffer 描述符：driver 提供可用 buffer，device/backend 消费后报告已使用 buffer。队列的实现细节可为 split ring 或 packed ring，但核心所有权转换不变。

```text
driver owns free buffers
  -> driver publishes descriptors to available side
  -> backend owns/uses the described buffers
  -> backend publishes completion to used side
  -> driver reclaims buffers
```

任何“谁能改 descriptor、谁能释放 buffer、何时可复用”的回答都应放在这个循环里，不要用“QEMU 把包传给 guest”这种模糊表述。

## split virtqueue 的最小结构

传统 split virtqueue 可以理解为三块共享区域：

| 区域 | 谁主要写 | 谁主要读 | 含义 |
| --- | --- | --- | --- |
| descriptor table | 准备 buffer 的一方 | 消费 buffer 的一方 | 地址、长度、next、读/写方向 |
| avail ring | driver | backend | 哪些 descriptor 已可消费 |
| used ring | backend | driver | 哪些 descriptor 已完成与实际长度 |

真实实现还涉及 index、wrap/ordering、indirect descriptor、DMA 映射和内存屏障。不要把“环”想成可无序并发写的普通数组。

## TX 与 RX 的所有权方向不同

### guest TX

1. guest `virtio_net` 从协议栈获得要发送的数据；
2. driver 把描述该数据的 buffer 放入 TX virtqueue avail；
3. backend 读取描述符，把帧送往 TAP/vhost 路径；
4. backend 在 used ring 报告完成；
5. driver 才能回收/释放该 buffer。

### guest RX

1. guest driver 预先向 RX virtqueue 提供可写 buffer；
2. backend 从 TAP/bridge 接到入站帧；
3. backend 选择一个可用 guest RX buffer，填入帧；
4. backend 写 used ring 并通知 guest；
5. guest NAPI/queue callback 回收，交给协议栈，再补新的 RX buffer。

RX buffer 不足、used ring 未被 guest 回收、backend 无法写入映射内存，都可能造成丢包或停滞；它们不能从单一 `ping` 结果推断。

## kick 与 call：通知不是数据本身

共享 ring 负责描述符内容；通知机制只负责提醒另一方“值得看看队列了”。常见实现使用 eventfd：

- **kick**：driver/guest 通知 backend，avail ring 有新工作；
- **call/interrupt**：backend 通知 driver/guest，used ring 有完成项。

高性能路径可通过 event index、batch、NAPI poll、`need_wakeup` 等策略减少无效通知。减少通知不是取消同步：发布 descriptor/index 与触发通知之间仍需要正确内存序。

## 内存序与可见性

队列两端必须保证：先写描述符和数据，再发布 avail index；消费端先观察到新 index，才能安全读取描述符。完成路径同理。virtio 内核/库提供相应 barrier 和 API；实验代码不应自行用普通变量替换这些同步点。

这种规则解释一个重要现象：偶尔能通的网络不证明队列实现正确。错误的可见性在轻负载、单 CPU 或特定缓存时序下可能被掩盖。

## queue 数量与控制队列

virtio-net 通常有 RX/TX data virtqueue；是否具备多个队列、控制队列、MQ/RSS/offload 取决于 feature negotiation。控制队列用于部分设备配置类操作，不应与每包 RX/TX 队列混为一谈。

扩展多队列时必须同时描述：

1. guest queue 与 host backend queue 的映射；
2. 每个 queue 的 CPU/IRQ/NAPI 亲和性；
3. 统计按 queue 聚合还是按设备汇总；
4. queue reset/stop 时所有权如何收回。

## 可证明与不可证明

| 观察 | 能证明 | 不能单独证明 |
| --- | --- | --- |
| guest `tx_packets` 增长 | guest 网卡统计认为发送发生 | 后端/bridge 已成功转发 |
| TAP 抓到帧 | 后端向 host TAP 注入或取出帧 | guest used ring 回收无误 |
| vhost 模块存在 | 内核具备模块/设备 | 该 QEMU 进程正在走 vhost 数据面 |
| ping 成功 | 双向协议路径在该时段可达 | 无丢包、无队列饥饿或无性能问题 |

本章是 [vhost 后端](06_VHOST_NET_AND_BACKEND_SWITCHING.md) 和 [性能测量](09_MULTIQUEUE_OFFLOAD_AND_PERFORMANCE.md) 的前置知识。
