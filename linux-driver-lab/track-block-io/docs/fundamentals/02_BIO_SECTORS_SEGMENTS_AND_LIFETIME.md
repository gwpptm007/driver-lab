# 02. bio、sector、segment 与生命周期

bio 是 block layer 的基本 I/O 描述：它指出目标 block device、逻辑位置、方向和一组内存段。它不是“一个连续 C buffer”，也不是稳定的 driver 私有对象。

## 2.1 地址单位必须明确

| 单位 | 作用 | 常见错误 |
| --- | --- | --- |
| sector | block layer 的逻辑地址单位，通常以 512-byte sectors 表示 | 把 sector 当 byte offset，产生 512 倍越界 |
| logical block size | 设备对外的最小逻辑 I/O 对齐 | 假设所有设备都是 4 KiB |
| byte length | 本次 bio/request 实际传输长度 | 忽略尾段/部分完成 |
| capacity | 设备可访问总范围 | 未检查 start + length 溢出/越界 |

ramdisk copy 的基本检查是：起始 sector 转换后的 byte offset 与 length 均在 backing store 内，且不会发生整数溢出。任何越界都应完成为错误，不能截断后继续返回成功。

## 2.2 segment 不是偶然细节

一个 bio 可以由多个 page/segment 组成。驱动需要按 block layer 提供的迭代方式遍历每段，按方向从 segment 拷入 backing store 或拷出。假设一个 request 只有一个线性 buffer，会在大 I/O、碎片页或不同内存布局下损坏数据。

对真实 DMA 驱动，segment 还涉及 DMA mapping、设备可寻址范围、IOMMU 与 scatter-gather 限制；教学 ramdisk 不做 DMA，但应保留“数据可能分段”的心智模型。

## 2.3 方向与所有权

读：后端数据写入 bio 描述的内存；写：bio 描述的内存复制到后端。驱动不能在完成后继续访问这些页面/segment。异步设备中，request/bio 的 buffer 直到对应完成前都不能被当作空闲。

官方 API 文档明确指出，提交 bio 后，调用方在 end_io 回调前不能再触碰该 bio。这个规则是后续 request、DMA 与 completion 设计的根本。

## 2.4 最小测试矩阵

- 首尾 sector、零长度与越界请求；
- 单 segment 与多 segment；
- 4 KiB、非 4 KiB 整数倍、不同 block size；
- 连续写后读回与随机 offset；
- 并发读写下的数据一致性策略。

下一篇：[03 request、blk-mq 与 tag](03_REQUESTS_BLK_MQ_TAGS_AND_COMPLETION.md)。
