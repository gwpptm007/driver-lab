# 09. 扩展路线与 virtio-blk 映射

启动实现后，不应从简单 ramdisk 直接跳到 NVMe 性能项目。正确路线是逐步增加真实约束，并保留前一阶段的测试。

## 9.1 阶段化演进

| 阶段 | 新增能力 | 必须保留的证据 |
| --- | --- | --- |
| Phase 1 ramdisk | device 生命周期、sector copy、基本完成 | raw I/O、mkfs/mount、cleanup |
| Phase 2 path | bio/request/trace 映射 | 输入到 completion 的时间线 |
| Phase 3 blk-mq | tag、hctx、queue depth、并发 | 同语义 workload 与错误处理 |
| Phase 4 virtio-blk 阅读 | probe、virtqueue、feature、completion | 函数/对象对照与运行期证据 |
| Phase 5 observability | fio、iostat、trace/eBPF、perf | 延迟、队列和 CPU 的关联 |
| Phase 6 summary | evidence、边界、面试材料 | 每个结论的环境与原始记录 |

## 9.2 ramdisk 与 virtio-blk 的对照

| 教学模型 | virtio-blk 真实概念 | 学习重点 |
| --- | --- | --- |
| backing memory | host/backend storage | 后端不再是简单 memcpy |
| blk-mq queue_rq | request 转为 virtqueue descriptor | request 所有权与异步投递 |
| 本地完成 | virtqueue used buffer/中断/轮询完成 | 完成可乱序、需匹配 request |
| 固定 capacity | config space 获取容量/features | 探测、协商与动态边界 |
| 单一锁/单队列 | 多队列与 CPU affinity | 并发、tag、NUMA 与性能 |

virtio-blk source dive 应先固定一个 kernel revision，再记录 probe、queue setup、request submit、kick、used buffer 处理和 remove 的函数映射。不要只列函数名；要说明每一步的 owner 和错误路径。

## 9.3 启动前设计检查表

- 目标 kernel、编译方式和可恢复测试环境是否固定？
- Phase 1 是否只实现能定义且能测试的 opcode？
- device 可见前，queue、capacity、backing memory 是否已完整？
- 卸载时是否先拒绝新 I/O、drain/quiesce，再释放内存？
- 每个性能结论是否明确为 ramdisk、虚拟设备或真实硬件？
- 是否有一份命令、记录和清理步骤可由他人复现？

本支线目前仍为计划状态。开始代码前应先将这些文档与目标 kernel 版本核对，并更新 [ROADMAP.md](../../ROADMAP.md) 的阶段状态。
