# DPDK vmxnet3 真网卡验证 — 完整方法论

**日期**: 2026-06-10
**测试机器**: wq7-virtual-machine (VMware Workstation, Ubuntu 22.04.5 LTS)
**内核**: 6.8.0-124-generic
**DPDK 版本**: 21.11

---

## 1. 目标

验证 DPDK 能否在真实 VMware vmxnet3 网卡上收发包（不仅是 pcap PMD 虚拟路径）。

## 2. 环境

### 2.1 测试机器（VM 内）

| 项目 | 值 |
|------|-----|
| SSH | `wq7@192.168.65.135` (port 33390, 通过 frp 映射) |
| 用户密码 | `wq123456!` |
| root 密码 | `wq123456!` |
| sudo 方式 | `echo 'wq123456!' | sudo -S` (非免密) |
| DPDK 源码 | `/home/wq7/workspace/dpdk-21.11` |
| fastpath-lite | `/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/project-user-space-fastpath/app/build/fastpath-lite` |

### 2.2 网卡清单

| 接口 | PCI 地址 | 驱动 | MAC | IP (kernel) | 类型 | VMnet |
|------|---------|------|-----|-------------|------|-------|
| ens33 | 0000:02:01.0 | e1000 (kernel) | 00:0C:29:F8:F6:6E | 192.168.65.135 | 管理口 | VMnet8 (NAT) |
| ens34 | 0000:02:02.0 | e1000 (kernel) | 00:0C:29:F8:F6:78 | — | 测试口 | hostonly (VMnet1) |
| ens192 | 0000:0b:00.0 | vmxnet3 | 00:0C:29:F8:F6:82 | 192.168.65.200 | **目标网卡** | VMnet3 → 后改为 VMnet8 |

### 2.3 Windows 主机

| 项目 | 值 |
|------|-----|
| VMware | VMware Workstation Pro |
| .vmx 路径 | `D:\software\install\VMware\Ubuntu22\vm\Ubuntu22-wq.vmx` |
| vmrun.exe | `D:\software\install\VMware\bin\vmrun.exe` |
| VMnet8 网关 | 192.168.65.1 |
| Windows VMnet8 IP | 192.168.65.x (DHCP) |
| SSH config | `~/.ssh/config` → `wq-ubuntu` → `wq7@192.168.65.135:33390` |

## 3. 实验完整过程

### 阶段 1: DPDK TX 路径验证（pcap PMD 作为流量源）

**原理**: pcap PMD 无限重放 pcap 文件（约 2000 个 UDP 包），fastpath-lite 做 classify/forward → 从 vmxnet3 端口 TX 出去。这样即使没有外部流量注入，也能验证 DPDK 通过真网卡的发包能力。

**步骤**:

```bash
# 1.1 SSH 到测试机器
ssh wq-ubuntu

# 1.2 生成测试 pcap（2000 个 UDP 包，dst MAC=vmxnet3 的 MAC）
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk
python3 project-dpdk-media-gateway-lite/tools/gen_udp_pcap.py /tmp/test_nic.pcap 2000

# 1.3 绑定 vmxnet3 到 UIO
echo 'wq123456!' | sudo -S modprobe uio_pci_generic
echo 'wq123456!' | sudo -S dpdk-devbind.py -b uio_pci_generic 0000:0b:00.0

# 1.4 启动 fastpath-lite（双端口: port0=vmxnet3, port1=pcap PMD）
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/project-user-space-fastpath
echo 'wq123456!' | sudo -S ./app/build/fastpath-lite \
  -l 0-1 -n 4 --file-prefix fp_nic_test \
  -a 0000:0b:00.0 \
  --vdev "net_pcap0,rx_pcap=/tmp/test_nic.pcap,infinite_rx=1" \
  -- \
  --run-seconds 8 --stats-period 2 --burst-size 32 --promisc 1

# 1.5 观察统计输出 → tx=5,538,176 on port 0 (vmxnet3)
```

**结果**: TX=553 万包，SW stats 与 ethdev stats 完全一致。证明 vmxnet3 PMD 发包路径正常。

### 阶段 2: 尝试外部流量注入（ens33/ens34 → vmxnet3）

**原理**: 在 DPDK 占用 vmxnet3 的同时，从 ens33（同 VMnet8 NAT 网段）或 ens34（hostonly）发送 UDP 包到 vmxnet3 的 MAC 地址，如果 DPDK RX 路径可用，应该能看到 rx 计数增长。

**步骤**:

```bash
# 2.1 先创建测试 pcap（2000 个 UDP 包）
python3 project-dpdk-media-gateway-lite/tools/gen_udp_pcap.py /tmp/test_nic2.pcap 2000

# 2.2 部署测试脚本到测试机
echo 'wq123456!' | sudo -S ip addr add 192.168.65.200/24 dev ens192
echo 'wq123456!' | sudo -S ip link set ens192 up

# 2.3 编写 scapy 发包脚本 send_from_ens33.py
cat > /tmp/send_udp.py << 'PYEOF'
import sys
from scapy.all import *

iface = sys.argv[1]
dst_mac = sys.argv[2]
dst_ip = sys.argv[3]
count = int(sys.argv[4])

pkt = Ether(dst=dst_mac) / IP(dst=dst_ip, src="192.168.65.100") / UDP(sport=9999, dport=9999) / Raw(b"X" * 64)
sendp(pkt, iface=iface, count=count, inter=0.001)
print(f"Sent {count} packets from {iface}")
PYEOF

# 2.4 从 ens34 发包到 vmxnet3 MAC
echo 'wq123456!' | sudo -S python3 /tmp/send_udp.py ens34 \
  00:0C:29:F8:F6:82 192.168.65.200 3000

# 2.5 从 ens33 发包到 vmxnet3 MAC
echo 'wq123456!' | sudo -S python3 /tmp/send_udp.py ens33 \
  00:0C:29:F8:F6:82 192.168.65.200 3000

# 结果: DPDK fastpath-lite rx=0（全部周期）
# 原因: VMnet 之间 L2 隔离，UDP 帧无法跨 VMnet 转发
```

### 阶段 3: VMX 文件修改（解决 VMnet 隔离）

**原理**: 将 ens192 从 VMnet3 改到 VMnet8（与 ens33 同网段），使外部流量可以到达该网卡。VMware 的 .vmx 文件存储了虚拟机的完整配置，修改后需要冷启动（cold boot）生效。

**VMX 修改前**:
```ini
ethernet2.connectionType = "custom"
ethernet2.vnet = "VMnet3"
ethernet2.displayName = "VMnet3"
```

**VMX 修改后**:
```ini
ethernet2.connectionType = "nat"
ethernet2.addressType = "generated"
ethernet2.generatedAddress = "00:0c:29:f8:f6:82"
```

**执行**（在 Windows 主机上）:

```powershell
# 3.1 硬关机（必须冷启动，热重启会还原 .vmx 修改）
& "D:\software\install\VMware\bin\vmrun.exe" -T ws stop "D:\software\install\VMware\Ubuntu22\vm\Ubuntu22-wq.vmx" hard

# 3.2 确认已关机
& "D:\software\install\VMware\bin\vmrun.exe" -T ws list

# 3.3 冷启动
& "D:\software\install\VMware\bin\vmrun.exe" -T ws start "D:\software\install\VMware\Ubuntu22\vm\Ubuntu22-wq.vmx" nogui
```

**验证**（VM 启动后）:
```bash
# SSH 登录后检查
ssh wq-ubuntu
ip addr show ens192
# → 192.168.65.200 (DHCP from VMnet8 NAT)

# 从 Windows ping 验证
ping 192.168.65.200
# → <1ms, L2 连通性确认
```

### 阶段 4: VMX 修改后再次尝试外部流量注入

```bash
# 4.1 重新绑定 vmxnet3 到 DPDK
echo 'wq123456!' | sudo -S dpdk-devbind.py -b uio_pci_generic 0000:0b:00.0

# 4.2 启动 fastpath-lite（单端口模式，只绑定 vmxnet3）
echo 'wq123456!' | sudo -S ./app/build/fastpath-lite \
  -l 0-1 -n 4 --file-prefix fp_rx_test \
  -a 0000:0b:00.0 \
  -- \
  --run-seconds 15 --stats-period 3 --burst-size 32 --promisc 1

# 4.3 同时从 ens33 发包
echo 'wq123456!' | sudo -S python3 /tmp/send_udp.py ens33 \
  00:0C:29:F8:F6:82 192.168.65.200 3000

# 4.4 同时从 ens34 发包
echo 'wq123456!' | sudo -S python3 /tmp/send_udp.py ens34 \
  00:0C:29:F8:F6:82 192.168.65.200 3000

# 结果: rx 仍然 = 0（所有周期）
```

### 阶段 5: 排查 RX=0 根因

**排查 1: PCI BusMaster**
```bash
lspci -s 0000:0b:00.0 -vvv | grep -i "BusMaster"
# → BusMaster- (被禁止)
# 修复:
echo 'wq123456!' | sudo -S setpci -s 0000:0b:00.0 COMMAND=0x07
# → BusMaster+ 但对 RX 无改善
```

**排查 2: 尝试 VFIO-PCI**
```bash
# 先解绑 UIO
echo 'wq123456!' | sudo -S dpdk-devbind.py -u 0000:0b:00.0

# 尝试 VFIO 绑定
echo 'wq123456!' | sudo -S modprobe vfio-pci
echo 'wq123456!' | sudo -S dpdk-devbind.py -b vfio-pci 0000:0b:00.0
# Error: [Errno 22] Invalid argument

# 检查 IOMMU
ls /sys/kernel/iommu_groups/
# → 空目录 — VMware guest 不支持 IOMMU

# 内核 cmdline 没有 intel_iommu=on
cat /proc/cmdline
# → BOOT_IMAGE=... ro quiet splash (无 IOMMU 参数)
```

**排查 3: 从 Windows 侧发包**
```powershell
# Npcap 未安装
python -c "from scapy.all import *; sendp(...)"
# → "No libpcap provider available"
# Windows 侧无法直接发包
```

**根因分析**:

```
vmxnet3 PMD RX 路径需要 MSI-X 中断来通知新包到达。
UIO (uio_pci_generic) 只提供基础 PCI 设备访问（MMIO、BAR），
不提供 MSI-X 中断支持。vmxnet3 的 RX 数据路径依赖中断通知，
因此 UIO 无法支持 vmxnet3 RX。

而 VFIO-PCI 通过 VFIO 框架提供完整的中断虚拟化，
但 VFIO 依赖 IOMMU 进行 DMA 地址转换和隔离。
VMware guest 不提供 IOMMU group，因此 VFIO 无法绑定。

TX 路径不需要中断（主动轮询 + 写 MMIO 寄存器），因此 UIO 下 TX 正常。
```

## 4. 关键命令速查

### 4.1 DPDK 环境准备

```bash
# hugepage
echo 'wq123456!' | sudo -S sh -c 'echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages'

# 加载 UIO
echo 'wq123456!' | sudo -S modprobe uio_pci_generic

# 网卡绑定
echo 'wq123456!' | sudo -S dpdk-devbind.py -b uio_pci_generic 0000:0b:00.0

# 查看状态
echo 'wq123456!' | sudo -S dpdk-devbind.py --status
```

### 4.2 fastpath-lite 运行

```bash
# 双端口: pcap PMD(port1) → vmxnet3(port0) TX
echo 'wq123456!' | sudo -S ./app/build/fastpath-lite \
  -l 0-1 -n 4 --file-prefix fp_nic_test \
  -a 0000:0b:00.0 \
  --vdev "net_pcap0,rx_pcap=/tmp/test_nic.pcap,infinite_rx=1" \
  -- \
  --run-seconds 8 --stats-period 2 --burst-size 32 --promisc 1

# 单端口: 纯 vmxnet3 RX 测试
echo 'wq123456!' | sudo -S ./app/build/fastpath-lite \
  -l 0-1 -n 4 --file-prefix fp_rx_test \
  -a 0000:0b:00.0 \
  -- \
  --run-seconds 15 --stats-period 3 --burst-size 32 --promisc 1
```

### 4.3 恢复内核驱动

```bash
echo 'wq123456!' | sudo -S dpdk-devbind.py -u 0000:0b:00.0
echo 'wq123456!' | sudo -S dpdk-devbind.py -b vmxnet3 0000:0b:00.0
echo 'wq123456!' | sudo -S ip link set ens192 up
echo 'wq123456!' | sudo -S ip addr add 192.168.65.200/24 dev ens192
```

### 4.4 Windows 主机操作

```powershell
# 关机/启动
& "D:\software\install\VMware\bin\vmrun.exe" -T ws stop "D:\software\install\VMware\Ubuntu22\vm\Ubuntu22-wq.vmx" hard
& "D:\software\install\VMware\bin\vmrun.exe" -T ws start "D:\software\install\VMware\Ubuntu22\vm\Ubuntu22-wq.vmx" nogui

# 查看运行状态
& "D:\software\install\VMware\bin\vmrun.exe" -T ws list
```

### 4.5 PCI 调试

```bash
# 查看 PCI 设备
lspci -nn | grep -i net

# 查看具体配置
lspci -s 0000:0b:00.0 -vvv

# 检查 BusMaster
lspci -s 0000:0b:00.0 -vvv | grep BusMaster

# 手动开启 BusMaster
setpci -s 0000:0b:00.0 COMMAND=0x07

# 检查 IOMMU group
ls /sys/kernel/iommu_groups/
```

## 5. 判定总结

| 判定 | 状态 | 证据 |
|------|------|------|
| PASS_INIT | vmxnet3 PMD 初始化成功 | driver=net_vmxnet3, rx_desc=1024, tx_desc=1024 |
| PASS_TX | 553 万包通过真网卡 TX | pcap PMD → fastpath-lite → vmxnet3 TX |
| PASS_STATS_CONSISTENCY | SW stats == ethdev stats | ipackets/opackets 与 rx/tx 完全匹配 |
| PASS_VMNET_FIX | .vmx 编辑 + 冷启动修改 VMnet 绑定 | Windows ping ens192 <1ms |
| BLOCKED_RX | vmxnet3 PMD RX 始终为 0 | UIO 不提供 MSI-X 中断, VFIO 需要 IOMMU |
| BLOCKED_E1000 | VMware 虚拟 82545EM 不兼容 DPDK | UIO/VFIO 均绑定失败 |

## 6. 测试拓扑

```
                         VMware Workstation (Windows)
  ┌──────────────────────────────────────────────────────────────────────┐
  │                                                                      │
  │   VMnet8 (NAT, 192.168.65.0/24)                                      │
  │   ├─ 192.168.65.1 (gateway)                                         │
  │   ├─ 192.168.65.x (Windows host adapter)                             │
  │   │                                                                  │
  │   └─ Ubuntu22-wq VM ──────────────────────────────────────────────┐  │
  │       ├─ ens33 (0000:02:01.0, e1000, 192.168.65.135) ← 管理口    │  │
  │       ├─ ens34 (0000:02:02.0, e1000, hostonly)   ← 空闲          │  │
  │       └─ ens192 (0000:0b:00.0, vmxnet3) ← DPDK 目标网卡          │  │
  │           原本在 VMnet3 → 编辑 .vmx 改为 VMnet8                   │  │
  │                                                                   │  │
  │  DPDK 占用 vmxnet3 时:                                             │  │
  │   ┌──────────────────────────────────────────────────┐            │  │
  │   │ fastpath-lite                                     │            │  │
  │   │  port0: vmxnet3 PMD (0000:0b:00.0)                │            │  │
  │   │    RX ──── 0 packets ❌ (UIO 无 MSI-X 中断)       │            │  │
  │   │    TX ──── 5,538,176 packets ✓                    │            │  │
  │   │  port1: pcap PMD (traffic generator)               │            │  │
  │   │    RX ──── 5,538,176 packets (pcap replay)        │            │  │
  │   │    classify/forward → port0 TX                    │            │  │
  │   └──────────────────────────────────────────────────┘            │  │
  └──────────────────────────────────────────────────────────────────────┘
```

## 7. 文件清单

| 文件 | 说明 |
|------|------|
| `REALNIC_TEST_REPORT.md` | 测试报告（本目录） |
| `METHODOLOGY.md` | 本文档 — 完整方法论、命令、环境记录 |
| `../REALNIC_TEST_REPORT.md` | 测试报告副本（records 根目录） |
| `../../../project-linux-network-data-plane/evidence/dpdk_evidence.md` | 证据索引（含真网卡验证真网卡验证章节） |

## 8. 扩展：若要在真实环境完成 RX 验证

需要**满足以下任一条件**:

1. **裸机**: 直接有 IOMMU，用 VFIO-PCI 绑定 vmxnet3
2. **KVM 虚拟机**: 启用 IOMMU passthrough（`intel_iommu=on vfio-pci`）
3. **VMware + SR-IOV**: 启用 VM DirectPath I/O 直通
4. **其他 DPDK 兼容 NIC**: e1000e (非 82545EM), ixgbe, i40e, mlx5 等有更好 UIO/VFIO 支持的驱动
