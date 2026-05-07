# track-dpdk

> DPDK 用户态数据面主线

## 一句话定位

从 vmxnet3/testpmd 起步，逐步进入 vhost-user、virtio-user、自写 L2 forwarding C app，最终收成 user-space fastpath / media gateway lite 项目。

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
| 6 | `project-fastpath-traffic-test` | `NEXT` | 真实 UDP 流量、UDP-only、rewrite、统计对照 |
| 7 | `project-dpdk-media-gateway-lite` | `PLANNED` | 简化媒体网关：规则表、双方向、按规则统计 |
| 8 | `project-dpdk-v17-legacy-review` | `PLANNED` | DPDK v17 旧项目经验和现代 DPDK 对照 |

## 推荐下一步

```bash
cd track-dpdk/project-fastpath-traffic-test
cat START_HERE.md
```

当前不要直接跳到 `project-dpdk-media-gateway-lite`。先把 `project-user-space-fastpath` 从 `PASS_SMOKE` 补到 `PASS_TRAFFIC/PASS_FORWARDING`。

## 文档结构

```text
track-dpdk/
├── README.md
├── ROADMAP_NEXT.md
├── docs/
│   ├── 00_ENVIRONMENT_PREPARE.md
│   ├── 01_OVERVIEW.md
│   ├── 02_ACCEPTANCE.md
│   ├── 03_CORE_CONCEPTS_AND_ARCHITECTURE.md
│   ├── 04_DPDK_V17_TO_MODERN_MAPPING.md
│   └── 05_NEXT_PROJECTS_ROADMAP.md
├── lab-*/
└── project-*/
```

## 快速入口

```bash
cd <lab-or-project-directory>
cat START_HERE.md
./scripts/00_check_env.sh
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
```
