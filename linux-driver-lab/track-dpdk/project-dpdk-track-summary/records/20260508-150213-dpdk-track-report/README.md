# track-dpdk
> 目录约定：`track-dpdk/` 根目录只放入口文件、路线文件、docs 和各 lab/project 目录；阶段性收口材料统一放在 `project-dpdk-track-summary/`。

> DPDK 用户态数据面主线

## 一句话定位

从 `vmxnet3/testpmd` 起步，逐步进入 `vhost-user`、`virtio-user`、自写 L2 forwarding C app、fastpath-lite、media-gateway-lite，最后用 v17 legacy review 和 track report 收成一条可讲清楚的用户态数据面能力线。

## 当前测试机环境

```text
Guest: Ubuntu 22.04.5 Desktop / Linux 6.8.0-110-generic
管理口: ens33 / e1000 / 192.168.65.135 (不动)
DPDK口: ens192 / vmxnet3 / 0000:0b:00.0 / uio_pci_generic
DPDK版本: 21.11.9
```

## 阶段状态

| 序号 | 项目 | 状态 | 核心验证 |
|------|------|------|----------|
| 1 | `lab-vmxnet3-testpmd` | `PASS` | hugepage + vmxnet3 + testpmd |
| 2 | `lab-vhost-user-basic` | `PASS` | vhost-user socket 创建 |
| 3 | `lab-virtio-user-vhost` | `PASS_WITH_WARN` | virtio-user + vhost-user 对接 |
| 4 | `lab-dpdk-l2-forwarding` | `PASS_SMOKE` | l2fwd-lite C app 编译/启动/stats |
| 5 | `project-user-space-fastpath` | `PASS_SMOKE` | fastpath-lite 协议分类/rewrite 框架，尚未证明真实流量 |
| 6 | `project-fastpath-traffic-test` | `READY_TO_TEST` | 真实 UDP 流量、UDP-only、rewrite、统计对照 |
| 7 | `project-dpdk-media-gateway-lite` | `PASS_SMOKE` | 简化媒体网关：双 vdev smoke + UDP-only drop path；真实流量后续补 |
| 8 | `project-dpdk-v17-legacy-review` | `PASS_REVIEW` | DPDK v17 旧项目经验、现代 DPDK 对照、面试/简历材料 |
| 9 | `project-dpdk-track-summary` | `READY` | track 总结、作品线、面试讲法、后续 backlog |

## 当前推荐下一步

当前主线已经进入“收口/作品化”阶段，推荐先读：

```bash
cat project-dpdk-track-summary/reports/final/DPDK_TRACK_REPORT.md
cat project-dpdk-track-summary/reports/final/DPDK_INTERVIEW_NOTES.md
cat project-dpdk-track-summary/reports/final/DPDK_RESUME_MATERIAL_FINAL.md
cat project-dpdk-track-summary/reports/final/DPDK_BACKLOG.md
```

如需生成可提交/归档的总结 bundle：

```bash
./project-dpdk-track-summary/scripts/00_make_track_report_bundle.sh
```

## 后续回补顺序

`project-dpdk-media-gateway-lite` 当前先记录为 `PASS_SMOKE`。后续要补：

```text
1. PASS_TRAFFIC: IPv4/UDP 收包统计非 0
2. PASS_FORWARDING: rule_hit / tx 非 0
3. PASS_REWRITE: rewrite_hit 非 0，并有统计或抓包证明
```

这部分放在 `project-dpdk-track-summary/reports/final/DPDK_BACKLOG.md` 中，不影响当前先完成 track 总结与简历材料。

## 文档结构

```text
track-dpdk/
├── README.md
├── ROADMAP_NEXT.md
├── project-dpdk-track-summary/
│   ├── reports/final/DPDK_TRACK_REPORT.md
│   ├── reports/final/DPDK_PROJECT_PORTFOLIO.md
│   ├── reports/final/DPDK_INTERVIEW_NOTES.md
│   ├── reports/final/DPDK_RESUME_MATERIAL_FINAL.md
│   └── reports/final/DPDK_BACKLOG.md
├── docs/
├── lab-*/
└── project-*/
```

## 最终能力线

```text
kernel netdev
  -> real driver observe/patch
  -> virtual net / virtio-vhost
  -> DPDK vmxnet3 PMD
  -> DPDK vhost-user / virtio-user
  -> l2fwd-lite C app
  -> fastpath-lite
  -> traffic-test
  -> media-gateway-lite
  -> DPDK v17 legacy review
  -> DPDK track report / interview / resume material
```
