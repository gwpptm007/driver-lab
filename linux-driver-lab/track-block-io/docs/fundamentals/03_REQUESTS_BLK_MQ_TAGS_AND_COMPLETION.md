# 03. request、blk-mq、tag 与 completion

block layer 可把一个或多个 bio 组合为 request，再把 request 提交给 driver。组合、调度和完成可能改变提交/完成顺序，因此 driver 不应把 userspace 调用顺序当作设备完成顺序。

## 3.1 blk-mq 的两级队列

| 对象 | 角色 |
| --- | --- |
| software context，ctx | CPU 近端的 software staging queue；用于合并、调度或暂存 |
| hardware context，hctx | 面向 driver/device submission queue 的 dispatch 上下文 |
| tag set | 描述硬件队列数、深度、保留 tag 与 driver operations |
| tag | block layer 为 request 分配的快速完成标识 |

直接派发不是保证：若可合并、存在 scheduler 或 driver 资源不足，request 可先停留在软件队列或 dispatch list。硬件队列数也不应机械等于 CPU 数，而要与后端并发能力、队列深度和 NUMA 拓扑匹配。

## 3.2 driver 的责任

一个 blk-mq driver 至少应定义 queue_rq 路径，并明确：

1. 何时开始 request，以便 block layer 计时；
2. 如何验证 opcode、范围和资源；
3. 如何处理每个 segment 或映射的 DMA；
4. 成功、失败、部分完成如何报告；
5. queue 满、暂时资源不足和 timeout 如何返回/重试；
6. shutdown/quiesce 时哪些 request 仍可能完成。

tag 是用于匹配完成的高效标识，不是业务顺序保证；同一队列请求的完成顺序也可能不同。上层文件系统负责需要的排序和一致性语义。

## 3.3 教学实现的建议

ramdisk 的第一可执行版本可以只提供单个 hardware queue、有限 queue depth 和同步或受控的异步完成，从而把状态机讲清楚。它不应通过无限深度或立即完成来掩盖资源/卸载问题。

在当前 kernel 上，优先将 Phase 1/3 的实现统一为明确的 blk-mq 教学路径；“简单”应指后端和并发模型简单，而不是回到已失配的 legacy API。官方细节见 [blk-mq 文档](https://docs.kernel.org/block/blk-mq.html)。
