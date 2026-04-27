# lab-vmxnet3-testpmd_report

## 目标

在当前 VMware Ubuntu22 测试机上跑通 DPDK 第一站：

```text
ens192/vmxnet3/0000:0b:00.0
hugepage
vfio-pci/uio
dpdk-devbind.py
dpdk-testpmd
port stats
records/report
```

## 测试机环境

| 项目 | 当前值 |
|------|--------|
| Guest | Ubuntu 22.04.5 Desktop |
| Kernel | Linux 6.8.0-110-generic |
| 管理网卡 | ens33 / e1000 / 192.168.65.135 |
| DPDK 网卡 | ens192 / vmxnet3 / 192.168.100.1/24 |
| DPDK PCI | 0000:0b:00.0 |
| 默认 DPDK driver | vfio-pci |

## 执行步骤

```bash
cd linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd
./scripts/00_check_env.sh
sudo ./scripts/01_setup_hugepages.sh
./scripts/02_bind_vmxnet3.sh status
sudo DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind
sudo ./scripts/03_run_testpmd.sh
./scripts/04_collect_stats.sh
./scripts/05_make_review_bundle.sh
```

## 验收标准

### 最低通过

- `ENV_CHECK.txt` 存在
- `HUGEPAGE_SETUP.txt` 或 `HUGEPAGE_STATUS.txt` 显示 hugepage 可用
- `BIND_STATUS.txt` 能显示 DPDK 设备状态
- `TESTPMD.log` 能看到 testpmd 启动输出

### 标准通过

- `ens33` 未被误操作
- `0000:0b:00.0` 成功由 DPDK driver 接管
- `TESTPMD.log` 有 port/stats 输出
- `REVIEW_BUNDLE.md` 生成

## 专家复盘点

1. DPDK 为什么优先确认 testpmd，而不是先写 app？
2. VMXNET3 的内核路径和 DPDK PMD 路径有什么不同？
3. hugepage 对 mempool/mbuf 的意义是什么？
4. bind 到 vfio-pci 后为什么 Linux 看不到普通 ens192 数据面？
5. 下一步为什么进入 vhost-user？

## 下一步

通过后进入：

```text
track-dpdk/lab-vhost-user-basic
```
