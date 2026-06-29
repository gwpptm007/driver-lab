# Block Layer Model

## 最小心智模型

```text
userspace I/O
  -> VFS / filesystem
  -> block device
  -> bio
  -> request
  -> request_queue or blk-mq
  -> driver backend
  -> completion
```

## 关键对象

| 对象 | 作用 |
|------|------|
| `gendisk` | 对外呈现一个 block device，如 `/dev/labram0` |
| major/minor | 设备号，用于把 `/dev/*` 映射到驱动 |
| `bio` | block layer 的基本 I/O 描述，包含 sector、方向、page/segment |
| `request` | 调度后提交给驱动的请求，可能由 bio 合并而来 |
| `request_queue` | 旧式 block request 队列入口 |
| `blk-mq` | 现代多队列 block I/O 框架 |
| completion | 驱动通知 block layer 请求完成 |

## Phase 1 先不覆盖的内容

- 不做真实 DMA。
- 不做真实硬件中断。
- 不做完整 page cache/writeback 深入分析。
- 不做 NVMe 多队列性能优化。

Phase 1 只要求用内存数组模拟后端，把 block device 生命周期和 read/write 闭环跑通。

