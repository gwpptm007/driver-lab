# 01_GOAL_AND_EXECUTION

## 项目目标

`project-user-space-fastpath` 是 `track-dpdk` 的收口项目，目标是形成项目链：

```
PMD 环境验证 → vhost/virtio-user → 自研 l2fwd → 业务型 fastpath
```

第一版 `fastpath-lite` 要掌握：
- DPDK 用户态数据面基本运行模型
- PMD / hugepage / mempool / mbuf / ethdev 关系
- RX burst / TX burst 收发循环
- ARP / IPv4 / UDP 最小解析
- 用户态 fastpath 中常见的分类、过滤、重写、统计

## 当前做什么

- 编译并运行自己的 DPDK C 程序
- 支持单端口 smoke 和双端口配对转发
- 支持 UDP-only 策略
- 支持 MAC / IP / UDP port rewrite
- 输出完整 records 和 review bundle

## 当前不做什么

- 不做多核 RSS/队列亲和优化
- 不做 ACL trie / flow table / timer wheel
- 不做完整 ARP responder
- 不做 KNI/TAP 回注内核
- 不追求性能极限

这些放到后续 `project-user-space-fastpath-v2` 或简历项目增强阶段。

## 测试机环境

```text
Guest: Ubuntu 22.04.5 Desktop
Kernel: Linux 6.8.0-110-generic
Hypervisor: VMware Workstation
管理网卡: ens33 / e1000 / 0000:02:01.0
DPDK 网卡: ens192 / vmxnet3 / 0000:0b:00.0
默认 driver: uio_pci_generic
HugePages: 1024 x 2MB
DPDK 版本: 21.11.9
```

**为什么默认不是 vfio-pci**：VMware Workstation guest 下 vfio-pci 因 IOMMU 条件不足容易失败，当前实验默认使用 `uio_pci_generic`。

**安全保护**：脚本保留了管理口保护，如果 `DPDK_PCI == MGMT_PCI` 会拒绝继续，避免误绑 `ens33` 导致 SSH 断开。

## 默认执行流程

```bash
cd project-user-space-fastpath

./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo ./scripts/02_prepare_vmxnet3.sh
sudo ./scripts/03_run_fastpath_single_port.sh
./scripts/07_collect_stats.sh
./scripts/08_make_review_bundle.sh
```

## vdev smoke 流程（不碰物理网卡）

```bash
./scripts/01_build_app.sh
sudo ./scripts/05_run_fastpath_vdev_null_pair.sh
sudo ./scripts/06_run_fastpath_rewrite_demo.sh
./scripts/08_make_review_bundle.sh
```

## 双口流程

当测试机有第二个 DPDK 口时：

```bash
sudo DPDK_PCI_1=0000:xx:yy.z ./scripts/04_run_fastpath_two_port.sh
```

## records 输出

```text
records/<timestamp>-user-space-fastpath/
├── SUMMARY.md
├── RESULT.md
├── COMMANDS.md
├── ENV_CHECK.txt
├── BUILD.log
├── PREPARE_VMXNET3.txt
├── FASTPATH_SINGLE_PORT.log
├── FASTPATH_VDEV_NULL_PAIR.log
├── FASTPATH_REWRITE_DEMO.log
├── COLLECT_STATS.txt
└── REVIEW_BUNDLE.md
```