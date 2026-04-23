# START_HERE_CURRENT

> 当前仓库主入口（基于 foundation/day01~day35 + netdev/stage00~stage14）

## 先读这几份

1. `README.md`
2. `POST_DAY35_MASTER_REVIEW_AND_ROADMAP.md`
3. `后续学习与项目推进计划_stage09后.md`
4. `netdev/README.md`
5. `track-real-driver/README.md`
6. `track-real-driver/lab-virtio-net-source-dive/README.md`

## 当前项目阶段

### 第一阶段：foundation（已收口）
- `foundation/day01~day35`
- 当前应视为冻结基线，不再继续线性追加 day36+

### 第二阶段：netdev（已推进到 stage14）
- `netdev/stage00~stage14`
- 关键覆盖：`net_device` / `skb` / NAPI / ring / multi-queue / MSI-X / page_pool / ethtool / offload / XDP 入口
- `stage14` 是 netdev 线性 stage 主线的收口点

### 第三阶段：track / lab / project（从 stage14 后开始）
- 不再继续扩成 `stage15 stage16 ...`
- 改为：`track-real-driver/`、`track-virtual-net/`、`track-perf-debug/`、`track-storage-block/`、`track-driver-core-pm/`

## 当前最推荐的下一步

- `track-real-driver/lab-virtio-net-source-dive/`

它承接你已经完成的 `netdev/stage00~stage14`，把“自己写教学驱动”推进到“阅读真实 Linux NIC 驱动源码”。


## 当前这个 Lab 怎么进入

- `track-real-driver/lab-virtio-net-source-dive/START_HERE.md`
- 先跑符号索引脚本，再看 probe/TX/RX，再做 stage 映射


## 进入真实驱动专题时，建议从这里开始

- `track-real-driver/lab-virtio-net-source-dive/START_HERE.md`
- `track-real-driver/lab-virtio-net-source-dive/docs/11_ROUND1_ARCH_CHECKLIST.md`
- `track-real-driver/lab-virtio-net-source-dive/docs/12_ROUND2_TXRX_CHECKLIST.md`
- `track-real-driver/lab-virtio-net-source-dive/docs/13_ROUND3_FEATURE_XDP_CHECKLIST.md`


## 当你想快速看当前 virtio_net 专题成果时

- `track-real-driver/lab-virtio-net-source-dive/docs/25_FINAL_SYNTHESIS_REPORT.md`
- `track-real-driver/lab-virtio-net-source-dive/docs/26_SHARE_DECK_SCRIPT.md`
- `track-real-driver/lab-virtio-net-source-dive/docs/27_PATCH_TRACING_PRIORITY_PLAN.md`
