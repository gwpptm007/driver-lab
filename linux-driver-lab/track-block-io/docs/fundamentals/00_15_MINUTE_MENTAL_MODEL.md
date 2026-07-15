# 00. 15 分钟心智模型

block I/O 处理的不是网络包，而是对一个线性、可寻址介质的读写请求。用户看到文件或块设备；驱动看到带方向、范围和内存片段的 I/O；设备最终执行读、写、flush 或管理命令。

~~~
userspace read/write/fio
  -> VFS / filesystem / page cache
  -> block device
  -> bio
  -> request
  -> blk-mq software context / hardware context
  -> driver backend or device queue
  -> completion
~~~

这条线和网络数据面共享 queue、DMA、completion、NUMA、backpressure 与可观测性问题，但多出三个存储语义：

- **地址与顺序**：sector 范围、合并、屏障和完成乱序；
- **持久性**：写入内存、设备接受、稳定介质和文件系统提交不是同一状态；
- **数据完整性**：长度、边界、错误传播、discard 和故障恢复不能靠吞吐掩盖。

## 当前支线的正确定位

Phase 1 的 ramdisk 是教学后端：用内存数组实现设备容量和 sector 读写，学习 gendisk、queue、request、completion 与安全卸载。它不包含真实 DMA、中断、NVMe 协议、介质持久性或生产级缓存/调度优化。

## 四个必须先回答的问题

| 问题 | 例子 |
| --- | --- |
| I/O 的逻辑范围是什么？ | sector、bytes、对齐与设备容量如何验证？ |
| 数据位于何处？ | page/segment、ramdisk backing memory、DMA buffer 谁拥有？ |
| 何时完成？ | driver copy 完成、request complete、flush 完成分别意味着什么？ |
| 失败时怎么办？ | 超范围、只读、内存不足、卸载期间 in-flight I/O 如何处理？ |

如果这四项说不清，就不应直接开始写 request callback。
