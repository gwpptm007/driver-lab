# track-block-io START_HERE

当前状态：`PARKED_PLANNED`。规划已落地，尚未开始实现代码。

当前主线已调整为：

```text
track-dpdk-advanced
  -> track-rdma
  -> track-smartnic-dpu
```

本目录先作为 P2 支线保留，后续需要补齐 block layer / storage I/O 时再启动。

建议阅读顺序：

```text
1. README.md
2. ROADMAP.md
3. docs/00_TRACK_OVERVIEW.md
4. docs/01_BLOCK_LAYER_MODEL.md
5. docs/02_IMPLEMENTATION_PLAN.md
6. docs/03_ACCEPTANCE.md
7. lab-simple-ramdisk/README.md
```

第一步从 `lab-simple-ramdisk` 开始：

```bash
cd linux-driver-lab/track-block-io/lab-simple-ramdisk
cat README.md
cat docs/01_LAB_OVERVIEW.md
```

本 track 的主线心智模型：

```text
userspace read/write/fio
  -> filesystem / block device
  -> bio
  -> request
  -> request_queue or blk-mq
  -> driver storage backend
  -> completion
```
