# DPDK Evidence

## 对应章节

- `../docs/04_DPDK_FASTPATH.md`

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

## 边界

pcap PMD path 已通过，不等于真实物理网卡双口生产压测完成。
