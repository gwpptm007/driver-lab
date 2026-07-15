# 03. 队列、内存与完成语义

高性能网络软件看上去由很多 API 组成，底层却反复出现同一结构：生产者把拥有权交给消费者；消费者在某个完成点后把资源还回去。ring、descriptor、doorbell 和 CQE 都是这套交接的具体形式。

## 3.1 队列的四个契约

每个队列必须写明：

| 契约 | 需要回答的问题 |
| --- | --- |
| 并发 | SP/SC、MP/SC、SP/MC 还是 MP/MC？谁能推进哪个索引？ |
| 容量 | 满时拒绝、丢弃、阻塞、降载还是扩容？ |
| 可见性 | descriptor 写完后如何发布？消费者何时可读？ |
| 顺序 | FIFO、flow affinity、乱序完成或每 key 有序？ |

DPDK RX/TX queue、rte_ring、AF_XDP rings、RDMA SQ/CQ 的具体 API 不同，但不回答这四项就无法安全调优。

## 3.2 Buffer 生命周期的通用状态机

最小模型可以写成：

~~~
FREE -> OWNED_BY_PRODUCER -> PUBLISHED -> OWNED_BY_DEVICE_OR_CONSUMER
     -> COMPLETED -> FREE
~~~

实际状态可更多：AF_XDP frame 会在 fill/RX/TX/completion rings 间移动；RDMA buffer 可能已 post 但尚未有 CQE；DPDK TX mbuf 可能等待硬件 reclaim。关键约束不变：**未进入可重用状态前，任何一方不得覆盖或释放该内存。**

## 3.3 descriptor 与 payload 分离

好的数据面通常只在共享队列中放小 descriptor：

~~~
descriptor: address/offset, length, flags, queue metadata, request or generation id
payload   : mbuf data, UMEM frame, registered MR, packet buffer
~~~

分离的好处是队列缓存友好、元数据可验证、payload 生命周期可独立管理。代价是 descriptor 的 address/offset/length 必须在发布前完整，且 consumer 必须验证范围。

当 slot 会复用而完成可能迟到时，descriptor 还需要 generation 或等价的版本标识。只用数组下标/slot ID 会让旧完成误释放新请求。

## 3.4 内存序不是可选优化

在无锁 SPSC 模型中，典型顺序是：

~~~
producer: 写 descriptor/payload -> release 发布 producer index
consumer: acquire 观察 index -> 读取 descriptor/payload

consumer: 完成读取/处理 -> release 发布 consumer index
producer: acquire 观察空闲 -> 覆盖旧位置
~~~

atomic 只解决可见性的一部分；若突然加入第二个 producer、把 descriptor 交给另一 lcore，或让两个线程 poll 同一 CQ，就必须重新选择队列模型和所有权，不能只增加一个 mutex 当作补丁。

## 3.5 背压是功能，不是失败

一个有限队列必然会满。健康设计把压力变成可观测、可控的信号：

| 压力位置 | 表现 | 可选策略 |
| --- | --- | --- |
| RX/应用 | buffer 不足、drop 增加 | 流控、采样、限速、增加容量 |
| local ring | enqueue 失败/occupancy 高 | producer 降载、分片队列、worker 扩展 |
| TX/SQ | descriptor/WR 预算耗尽 | poll/reclaim、限制 outstanding、批量化 |
| CQ/completion | poll 不及、completion backlog | 专用 core、批量 poll、调整 signaling |
| 远端 | RTT/完成变慢 | 限制 in-flight、超时状态机、业务级降级 |

把 ring 调大只延后背压出现，不能修复稳定吞吐低于输入速率的问题。

## 3.6 关闭与错误路径

安全关闭顺序通常是：停止新生产 -> drain 已发布工作 -> 等待/处理设备完成或 flush -> 销毁队列/内存注册 -> 释放内存。反过来释放 buffer 再等 CQE，会产生 DMA/use-after-free 风险。

错误 CQE、TX error、queue reset 或 offload 下发失败都应保留可关联的 identifier、时间和资源状态。对于语义未知的远端写入，不能因为本地超时就把 buffer 当作已失败且可立即复用。

下一篇：[04：控制面、数据面与观测面](04_CONTROL_DATA_OBSERVABILITY_PLANES.md)。
