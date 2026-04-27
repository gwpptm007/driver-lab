# 01_GOAL_AND_SCOPE

## 目标

本 Lab 是 `track-dpdk/` 的第一站，目标不是马上写业务转发程序，而是先把 DPDK 的运行底座跑通：

```text
VMware VMXNET3 网卡
hugepage
uio_pci_generic
dpdk-devbind.py
dpdk-testpmd
port stats
records/report
```

## 测试机环境

来自 `track-dpdk/docs/00_ENVIRONMENT_PREPARE.md`：

| 项目 | 值 |
|------|-----|
| VM 名称 | Ubuntu22-wq |
| Guest OS | Ubuntu 22.04.5 Desktop |
| Kernel | Linux 6.8.0-110-generic |
| User | wq7 |
| 管理IP | 192.168.65.135 |

## 网卡角色

| 接口 | PCI | 驱动 | IP | 用途 |
|------|-----|------|----|------|
| `ens33` | `0000:02:01.0` | e1000 | `192.168.65.135/24` | SSH/NAT 管理 |
| `ens34` | `0000:02:02.0` | e1000 | 无 | 备用 |
| `ens192` | `0000:0b:00.0` | vmxnet3 | `192.168.100.1/24` | DPDK 测试口 |

## 本 Lab 固定默认值

```bash
export DPDK_IF=ens192
export DPDK_PCI=0000:0b:00.0
export DPDK_DRIVER=uio_pci_generic
export MGMT_IF=ens33
```

> ⚠️ 注意：VMware Workstation 不支持 IOMMU，`vfio-pci` 无法使用。实际使用 `uio_pci_generic` 作为 DPDK 驱动。

## 为什么不动 ens33

`ens33` 是 NAT 管理网卡，也是远程 SSH 入口。  
如果误把 `ens33` bind 到 DPDK 驱动，普通 Linux 网络栈会失去这张网卡，SSH 会断。

所以本 Lab 的脚本有三层保护：

1. 默认 PCI 只写 `0000:0b:00.0`
2. bind 前检查 `DPDK_PCI` 不等于管理网卡 PCI
3. bind 动作必须显式传 `DPDK_CONFIRM_BIND=YES`

## 核心边界

```text
ens33 继续用于 SSH/NAT 管理，不参与 DPDK bind
ens192 是 DPDK 测试口，可以在 kernel vmxnet3 与 uio_pci_generic 之间切换
```

## 当前做什么

- 固化测试机环境假设
- 建立 DPDK 只读检查脚本
- 建立 hugepage 配置脚本
- 建立 vmxnet3 bind/unbind/status 脚本
- 建立 testpmd 运行脚本
- 建立 stats/records/review bundle 收集脚本
- 输出可复盘的验收模板

## 当前不做什么

- 不写 L2 forwarding C 程序
- 不做 vhost-user
- 不做 virtio-user
- 不改 `ens33` 管理网卡
- 不把 sudo 密码写进脚本
- 不在基础环境未跑通前追求性能指标

## 为什么这一步必须先做

DPDK 的学习顺序不能一开始就跳到业务代码。  
如果以下基础没有闭环，后续自写 C app 的问题会变成混乱的环境问题：

```text
hugepage 不够
vfio 权限不对
IOMMU 不清楚
网卡 bind 错了
testpmd 起不来
端口 stats 没有
```

所以本 Lab 的价值是：先建立“设备能被 DPDK 接管，并能由 testpmd 跑起来”的确定性。
