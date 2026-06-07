# Real Driver Evidence

## 对应章节

- `../docs/02_REAL_DRIVER_PATH.md`

## 主入口

- `../../track-real-driver/README.md`

## 关键证据

### virtio_net source dive

- `../../track-real-driver/lab-virtio-net-source-dive/README.md`
- `../../track-real-driver/lab-virtio-net-source-dive/docs/`
- `../../track-real-driver/lab-virtio-net-source-dive/reports/virtio_net_maturity_assessment.md`

### runtime observe

- `../../track-real-driver/lab-virtio-net-runtime-observe/README.md`
- `../../track-real-driver/lab-virtio-net-runtime-observe/docs/`

### ethtool stats mini patch

- `../../track-real-driver/lab-virtio-net-ethtool-stats-mini-patch/README.md`
- `../../track-real-driver/lab-virtio-net-ethtool-stats-mini-patch/reports/ethtool_patch_report.md`
- `../../track-real-driver/lab-virtio-net-ethtool-stats-mini-patch/records/`

### queue poll observe

- `../../track-real-driver/lab-virtio-net-queue-poll-observe/README.md`
- `../../track-real-driver/lab-virtio-net-queue-poll-observe/reports/queue_poll_report.md`

### final patch project

- `../../track-real-driver/project-virtio-net-patch-and-trace/README.md`
- `../../track-real-driver/project-virtio-net-patch-and-trace/reports/final_project_report.md`
- `../../track-real-driver/project-virtio-net-patch-and-trace/reports/review_bundle.md`
- `../../track-real-driver/project-virtio-net-patch-and-trace/records/20260425_203801-virtio-net-patch-trace/`

### e1000e compare

- `../../track-real-driver/lab-e1000e-source-compare/README.md`
- `../../track-real-driver/lab-e1000e-source-compare/reports/e1000e_compare_report.md`
- `../../track-real-driver/lab-e1000e-source-compare/reports/virtio_vs_e1000e_matrix.md`

## 已证明

```text
virtio_net 源码阅读 Round1~3
运行期 TX/RX 观测
ethtool stats mini patch before/after
napi_poll -> netif_receive_skb trace 链
virtio_net 与 e1000e 对照
```

## 边界

patch 属于低风险 stats/control plane 类型，不是生产驱动深度重构。
