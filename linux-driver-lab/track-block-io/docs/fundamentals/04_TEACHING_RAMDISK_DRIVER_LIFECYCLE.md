# 04. 教学 ramdisk driver 生命周期

ramdisk 的目标是把 block device 的资源关系和完成路径跑通，不是模仿真实设备速度。设计应先有资源表，再写 init/exit。

## 4.1 最小资源图

| 资源 | 责任 |
| --- | --- |
| backing memory | 固定容量的内存后端，承载 sector 数据 |
| major/minor 或设备注册资源 | 将 block device 暴露给系统 |
| gendisk | capacity、名称、fops 与 queue 的设备表示 |
| request queue / tag set | block layer 与 driver 的提交契约 |
| queue limits | logical block size、最大传输等边界 |
| driver private state | 锁、统计、卸载状态、backend 指针 |

资源创建必须能逐步回滚：任一步失败，只销毁已成功创建的对象；不能留下已注册但没有有效 queue/backing memory 的 device。

## 4.2 初始化顺序

建议顺序是：验证 module 参数 -> 分配和清零 private state/backing memory -> 初始化 tag set 和 queue -> 设置 queue limits -> 分配/初始化 gendisk -> 设置 capacity -> add disk/让设备可见。设备可见必须是最后一步，否则 userspace 可能在 driver 尚未完整初始化时发 I/O。

## 4.3 卸载顺序

卸载是反向的，但要多做一步：先禁止新 I/O 并 quiesce queue，等待/处理已经派发的 request，再让 disk 不可见、释放 gendisk/queue/tag set，最后释放 backing memory。不能先 free 内存再注销设备。

模块卸载前必须卸载文件系统、停止 fio/dd，并确认没有进程持有该 block device。教学环境应把这些前置条件写入命令和记录。

## 4.4 并发策略

第一阶段可选择单队列、单一 backend lock，以正确性优先；但锁的范围要明确，不要在持锁状态调用可能触发复杂 completion 的路径。之后再把并发扩展为多 hctx、per-queue state 或更真实后端。

要记录读/写、bytes、错误、in-flight、高水位和卸载拒绝数。这些计数同时是测试证据和资源泄漏检测工具。

## 4.5 不应实现的假能力

- backing memory 不具备掉电持久性；
- memcpy 后完成不代表真实 DMA/中断模型；
- mkfs/mount 通过不代表已支持崩溃恢复；
- fio 跑得快主要反映内存和 CPU，不能用于声明磁盘/NVMe 性能。

下一篇：[05 flush、discard、错误与完整性](05_FLUSH_FUA_DISCARD_ERRORS_AND_INTEGRITY.md)。
