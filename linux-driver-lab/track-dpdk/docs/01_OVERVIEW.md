# 01_OVERVIEW

## 目标

从 vmxnet3/testpmd 起步，逐步进入 vhost-user、virtio-user、自写 L2 forwarding C app，最终收成 user-space fastpath 项目。

承接前面已完成的内容：
- kernel netdev
- real driver
- virtual net

## 当前测试机环境

```text
Guest: Ubuntu 22.04.5 Desktop / Linux 6.8.0-110-generic
管理口: ens33 / e1000 / 192.168.65.135 (不动)
DPDK口: ens192 / vmxnet3 / 0000:0b:00.0 / uio_pci_generic
DPDK版本: 21.11.9
```

## 路线图

### Phase 1: `lab-vmxnet3-testpmd`

跑通 hugepage、uio、device bind、testpmd、port stats。

验收：有 records、有 report、有 review bundle

### Phase 2: `lab-vhost-user-basic`

把 vhost_net 视角推进到 DPDK vhost-user socket。

验收：有 records、有 report、有 review bundle

### Phase 3: `lab-virtio-user-vhost`

不依赖完整 VM，理解用户态 virtio frontend 与 vhost backend。

验收：有 records、有 report、有 review bundle

### Phase 4: `lab-dpdk-l2-forwarding`

实现最小 EAL/mempool/port/queue/rx_burst/tx_burst L2 forwarding。

验收：有 records、有 report、有 review bundle

### Phase 5: `project-user-space-fastpath`

整合 vmxnet3/testpmd、vhost-user、virtio-user、L2 forwarding 成项目。

验收：有 records、有 report、有 review bundle

## 快速开始

```bash
# 进入第一个实验
cd lab-vmxnet3-testpmd
cat START_HERE.md

# 环境检查
./scripts/00_check_env.sh
```

```bash
# 进入已完成实验
cd lab-dpdk-l2-forwarding
cat START_HERE.md
```

## VMware Type-2 限制

当前测试机使用 VMware Workstation (Type-2 Hypervisor)，不支持 IOMMU passthrough：
- 使用 `uio_pci_generic` 而非 `vfio-pci`
- DPDK IOVA 模式自动选择 PA（物理地址）

## 依赖工具

```bash
# 基础依赖
sudo apt install -y dpdk dpdk-dev libdpdk-dev

# 编译依赖（DPDK 21.11+ 使用 meson）
sudo apt install -y meson ninja-build pkg-config build-essential

# 可选工具
sudo apt install -y pcaplib-devel  # 支持 pdump 等
```