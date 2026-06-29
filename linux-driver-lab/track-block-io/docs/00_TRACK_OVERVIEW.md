# Block I/O Track Overview

## 背景

`project-linux-network-data-plane` 已经把 Linux 网络数据面收成一个作品集。下一步不继续堆网络实验，而是把高性能 I/O 能力迁移到 block layer / storage I/O。

## 核心问题

```text
一个 userspace read/write/fio 请求如何进入 Linux block layer？
bio、request、request_queue、gendisk、blk-mq 分别承担什么职责？
驱动如何完成数据搬运、请求完成和错误处理？
如何用 fio、iostat、blktrace、bpftrace 解释吞吐和延迟？
```

## 能力地图

| 层级 | 关注点 |
|------|--------|
| userspace | `dd`, `fio`, `mkfs`, `mount` |
| filesystem/block device | 文件系统到 block device 的边界 |
| block layer | `bio`, `request`, `request_queue`, elevator/merge 边界 |
| driver model | `gendisk`, major/minor, open/release, request handling |
| blk-mq | tag, hardware queue, software queue, completion |
| observability | fio, iostat, blktrace, bpftrace, perf |
| real driver | `virtio_blk` / later NVMe source dive |

## 与 network data plane 的关系

```text
netdev skb/ring/NAPI/completion
  <-> block bio/request/queue/completion

DPDK/AF_XDP polling
  <-> fio workload + blk-mq queueing + completion analysis

eBPF observability
  <-> block tracepoints / bpftrace / blktrace
```

## 当前阶段

当前只落地 track 规划和 Phase 1 入口。第一段实现从 `lab-simple-ramdisk` 开始。

