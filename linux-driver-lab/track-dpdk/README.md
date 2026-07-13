# track-dpdk
> 目录约定：`track-dpdk/` 根目录只放入口文件、路线文件、docs 和各 lab/project 目录；阶段性收口材料统一放在 `project-dpdk-track-summary/`。

> DPDK 用户态数据面主线

## 第一次进入这里

不要直接从项目命令开始。先打开 [`START_HERE.md`](START_HERE.md)，根据“零基础、快速复习、环境实验、C fast path”选择路径。

基础原理层已经按统一模板拆分到 [`docs/fundamentals/`](docs/fundamentals/README.md)：

```text
10 分钟心智模型
  -> 内核/NIC/用户态位置
  -> hugepage/mempool/mbuf
  -> RX 到 TX 完整数据路径
  -> queue/lcore/ownership 生命周期
  -> 项目知识地图
  -> 排障手册与复习卡
```

如果已经做过 DPDK、只是忘记上下文，推荐快速阅读：

1. [`00_10_MINUTE_MENTAL_MODEL.md`](docs/fundamentals/00_10_MINUTE_MENTAL_MODEL.md)
2. [`03_END_TO_END_DATA_PATH.md`](docs/fundamentals/03_END_TO_END_DATA_PATH.md)
3. [`07_RECALL_CARDS.md`](docs/fundamentals/07_RECALL_CARDS.md)

进一步写 parser、配置 port 或分析性能时，继续阅读 `08` 到 `12`：packet/offload、ethdev capability、性能观测、virtio/vhost 和安全环境准备。

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
| 5 | `project-user-space-fastpath` | `PASS_PCAP_FUNCTIONAL` | pcap PMD 输入下协议分类、forward/rewrite 计数守恒 |
| 6 | `project-fastpath-traffic-test` | `PASS_PCAP_FUNCTIONAL` `PASS_PCAP_FORWARDING` `PASS_PCAP_REWRITE` | `net_pcap -> fastpath -> net_null` 确定性功能流量 |
| 7 | `project-dpdk-media-gateway-lite` | `PASS_PCAP_FUNCTIONAL` `PASS_PCAP_FORWARDING` `PASS_PCAP_REWRITE` | pcap PMD 下 UDP rule、转发和 rewrite 闭环 |
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

## 后续硬件补验顺序

pcap/vdev 功能闭环已经完成。后续不能继续沿用模糊的“真实流量”表述，而应按证据层级补：

```text
1. PASS_EXTERNAL_TRAFFIC: 外部发包源进入目标端口
2. PASS_REAL_NIC_FORWARDING: 真实 NIC 双口 RX/TX 与抓包守恒
3. PASS_PERFORMANCE: 固定硬件、包长和方法下的 Mpps/Gbps/latency/cycles-per-packet
```

这部分放在 `project-dpdk-track-summary/reports/final/DPDK_BACKLOG.md`，不把 pcap replay 数字包装成线速或真实 NIC 性能。

## 文档结构

```text
track-dpdk/
├── README.md
├── START_HERE.md
├── ROADMAP_NEXT.md
├── docs/fundamentals/       # 进入项目之前的统一原理层
├── project-dpdk-track-summary/
│   ├── reports/final/DPDK_TRACK_REPORT.md
│   ├── reports/final/DPDK_PROJECT_PORTFOLIO.md
│   ├── reports/final/DPDK_INTERVIEW_NOTES.md
│   ├── reports/final/DPDK_RESUME_MATERIAL_FINAL.md
│   └── reports/final/DPDK_BACKLOG.md
├── docs/                    # 路线、验收和历史状态文档
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
