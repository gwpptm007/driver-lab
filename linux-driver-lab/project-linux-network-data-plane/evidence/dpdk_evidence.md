# DPDK Evidence

## 对应章节

- `../docs/04_DPDK_FASTPATH.md`
- `../docs/10_CROSS_PATH_COMPARISON.md` (三种路径横向对比)

## 主入口

- `../../track-dpdk/README.md`
- `../../track-dpdk/project-dpdk-track-summary/reports/final/DPDK_TRACK_REPORT.md`

## 关键证据

### Track summary

- `../../track-dpdk/project-dpdk-track-summary/reports/final/DPDK_TRACK_REPORT.md`
- `../../track-dpdk/project-dpdk-track-summary/reports/final/DPDK_RESUME_MATERIAL_FINAL.md`

### user-space fastpath

- `../../track-dpdk/project-user-space-fastpath/START_HERE.md`
- `../../track-dpdk/project-user-space-fastpath/reports/project-user-space-fastpath_report.md`
- `../../track-dpdk/project-user-space-fastpath/scripts/`

### media gateway lite

- `../../track-dpdk/project-dpdk-media-gateway-lite/reports/project-dpdk-media-gateway-lite_report.md`
- `../../track-dpdk/project-dpdk-media-gateway-lite/records/20260607-pcap-traffic-test/`

### fastpath traffic test

- `../../track-dpdk/project-fastpath-traffic-test/reports/project-fastpath-traffic-test_report.md`
- `../../track-dpdk/project-fastpath-traffic-test/records/20260607_155955-fastpath-pcap/`
- `../../track-dpdk/project-fastpath-traffic-test/records/20260607_160006-fastpath-pcap-rewrite/`

## 已证明

```text
DPDK 21.11 环境
hugepage / devbind / uio_pci_generic
vmxnet3 PMD / testpmd
vhost-user / virtio-user
l2fwd-lite / fastpath-lite
media-gateway-lite: PASS_TRAFFIC / PASS_FORWARDING / PASS_REWRITE
fastpath-traffic-test: PASS_TRAFFIC / PASS_FORWARDING / PASS_REWRITE
```

## 核心测试结论

`project-dpdk-media-gateway-lite` 已验证：

```text
PASS_BUILD
PASS_SMOKE
PASS_RULE_CONFIG
PASS_TRAFFIC
PASS_FORWARDING
PASS_REWRITE
```

## 真网卡验证 (vmxnet3 PMD)

- `../../track-dpdk/project-user-space-fastpath/records/realtest_vmxnet3_pcap_20260610_225500/REALNIC_TEST_REPORT.md`
- `../../track-dpdk/project-user-space-fastpath/records/realtest_vmxnet3_pcap_20260610_225500/METHODOLOGY.md`

### 方案

由于 VMnet 隔离导致外部流量无法注入 DPDK 控制的 vmxnet3，采用 pcap PMD 作为流量源：

```text
pcap PMD (port 1) → fastpath-lite classify/forward → vmxnet3 PMD (port 0, real NIC)
```

### 结果

| 指标 | 值 |
|------|-----|
| vmxnet3 TX packets | 5,538,176 |
| vmxnet3 TX bytes | 426,439,552 |
| throughput | ~692K pps |
| vmxnet3 PMD driver | net_vmxnet3 (init OK) |
| SW stats vs ethdev | CONSISTENT |

### 判定

```text
PASS_INIT            — vmxnet3 PMD 初始化成功
PASS_TX              — 553 万包通过真网卡 TX 发送
PASS_STATS_CONSISTENCY — 软件统计与 ethdev 硬件统计一致
BLOCKED_RX           — UIO 不提供 MSI-X 中断，VFIO 需要 IOMMU，VMware guest 无 IOMMU
BLOCKED_E1000        — VMware 虚拟 e1000 (82545EM) 无法绑定 UIO/VFIO
```

## 边界

pcap PMD path 已通过，不等于真实物理网卡双口生产压测完成。

vmxnet3 TX 路径已验证（553 万包），RX 路径因 UIO 无 MSI-X 中断 + VMware 无 IOMMU 标记为 BLOCKED_RX，本阶段不继续测试。
e1000 (VMware 虚拟 82545EM) 不兼容 DPDK UIO/VFIO。

详见 METHODOLOGY.md — 完整方法、命令、根因分析。
