# track-block-io Roadmap

## Phase 1: lab-simple-ramdisk

目标：写一个最小可用的内存块设备。

交付：

- block device 注册与卸载。
- read/write 数据路径。
- `mkfs.ext4`、`mount`、`dd`、`fio` smoke。
- `records/` 中保存命令、dmesg、lsblk、fio 输出。

## Phase 2: lab-bio-request-path

目标：解释 userspace I/O 如何变成 block layer 请求。

交付：

- bio/request/request_queue 路径文档。
- driver 中关键 callback 映射。
- read/write completion 证据。

## Phase 3: lab-blk-mq-ramdisk

目标：迁移到 blk-mq，理解 tag、hardware queue、software queue、completion。

交付：

- blk-mq 版本 ramdisk。
- 单队列与多队列配置说明。
- fio smoke 对比。

## Phase 4: lab-virtio-blk-source-dive

目标：把自写模型映射到真实 `virtio_blk`。

交付：

- probe / disk / queue / virtqueue / completion 源码阅读。
- virtio-blk 与 lab ramdisk 对照矩阵。
- 运行期观测记录。

## Phase 5: lab-fio-latency-observe

目标：建立 block I/O 可观测能力。

交付：

- fio workload 模板。
- iostat/blktrace/bpftrace/perf 采集脚本。
- latency/throughput 报告。

## Phase 6: project-block-io-track-summary

目标：把 block I/O 主线收成作品。

交付：

- final report。
- evidence index。
- interview notes。
- resume material。
- backlog。

