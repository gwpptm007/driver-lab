# 06_EXECUTION_RECORD

> 本文档记录 lab-vmxnet3-testpmd 实验的完整执行过程和结果。

## 实验时间线

| 步骤 | 脚本 | 状态 | 说明 |
|------|------|------|------|
| 1 | `00_check_env.sh` | ✅ 已完成 | 环境检查 |
| 2 | `01_setup_hugepages.sh` | ✅ 已完成 | 大页配置（1024 × 2MB = 2GB） |
| 3 | `02_bind_vmxnet3.sh` | ✅ 已完成 | 绑定到 uio_pci_generic |
| 4 | `03_run_testpmd.sh` | ✅ 已完成 | testpmd 冒烟测试通过 |
| 5 | `04_collect_stats.sh` | ✅ 已完成 | 收集统计信息 |
| 6 | `05_make_review_bundle.sh` | ✅ 已完成 | 生成 review bundle |

---

## 步骤 1：环境检查（00_check_env.sh）

**执行时间**：2026-04-26 21:34:01
**执行结果**：✅ 通过

### 环境快照

```text
## Lab 默认参数
LAB_ROOT=/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd
DPDK_IF=ens192
DPDK_PCI=0000:0b:00.0
DPDK_DRIVER=vfio-pci
MGMT_IF=ens33
MGMT_PCI=0000:02:01.0
HUGEPAGES=1024
HUGEPAGE_MOUNT=/mnt/huge
```

### 主机信息

```text
$ uname -a
Linux wq7-virtual-machine 6.8.0-110-generic #110~22.04.1-Ubuntu SMP PREEMPT_DYNAMIC
x86_64 x86_64 x86_64 GNU/Linux

OS: Ubuntu 22.04.5 LTS (jammy)
User: wq7 (uid=1000, gid=1000)

CPU: AMD Ryzen 9 7945HX with Radeon Graphics (8 cores, 2 sockets)
     AMD-V 虚拟化: 已启用
     NUMA: 1 node, CPUs 0-7
```

### 网卡状态

| 接口 | IP | 驱动 | PCI | 状态 |
|------|-----|------|------|------|
| `ens192` | `192.168.100.1/24` | vmxnet3 (1.7.0.0-k-NAPI) | `0000:0b:00.0` | ✅ UP |
| `ens33` | `192.168.65.135/24` | e1000 | `0000:02:01.0` | ✅ UP (管理口) |
| `ens34` | 无 | e1000 | `0000:02:02.0` | ✅ UP |

```text
$ lspci -s 0000:0b:00.0 -nn
0b:00.0 Ethernet controller [0200]: VMware VMXNET3 Ethernet Controller [15ad:07b0] (rev 01)
```

### 大页状态

```text
$ cat /proc/meminfo | grep Huge
HugePages_Total:    1024
HugePages_Free:     1024
Hugepagesize:       2048 kB
Hugetlb:         2097152 kB

$ mount | grep huge
hugetlbfs on /dev/hugepages type hugetlbfs (rw,relatime,pagesize=2M)
nodev on /mnt/huge type hugetlbfs (rw,relatime,pagesize=2M)
```

> ✅ 大页已预配置：1024 × 2MB = 2GB

### vfio-pci 状态

```text
$ lsmod | grep -E "vfio|uio|vmxnet3"
vmxnet3  94208  0
```

> ⚠️ `vfio-pci` 内核模块未加载，当前网卡绑定在 `vmxnet3` 驱动

### DPDK 工具

> ✅ DPDK 安装验证

**安装过程记录：**
- ❌ 错误命令：`sudo apt install -y dpdk-devtools dpdk-testpmd`（CentOS/RHEL 包名，Ubuntu 不存在）
- ✅ 正确命令：`sudo apt install -y dpdk dpdk-dev`

**⚠️ apt 源网络超时问题：**

1. **问题现象**：使用 `cn.archive.ubuntu.com` 和 `mirrors.tuna.tsinghua.edu.cn` 下载 DPDK 时连接超时
   ```
   E: Failed to fetch http://mirrors.tuna.tsinghua.edu.cn/ubuntu/pool/... Connection timed out
   E: Failed to fetch http://security.ubuntu.com/ubuntu/pool/... Connection refused
   ```

2. **解决方法**：换用阿里云源
   ```bash
   # 备份原有源
   sudo cp /etc/apt/sources.list /etc/apt/sources.list.bak

   # 换成阿里云源
   sudo sed -i 's|cn.archive.ubuntu.com|mirrors.aliyun.com|g' /etc/apt/sources.list
   sudo sed -i 's|http://mirrors.tuna.tsinghua.edu.cn/ubuntu|http://mirrors.aliyun.com/ubuntu|g' /etc/apt/sources.list

   # 更新并安装
   sudo apt update
   sudo apt install -y dpdk dpdk-dev
   ```

   或腾讯云源：
   ```bash
   sudo sed -i 's|cn.archive.ubuntu.com|mirrors.cloud.tencent.com|g' /etc/apt/sources.list
   sudo apt update
   sudo apt install -y dpdk dpdk-dev
   ```

3. **安装结果**：✅ 换源后成功安装 DPDK 21.11.9

**DPDK 版本验证：**
```
$ dpdk-testpmd -v
EAL: Detected CPU lcores: 8
EAL: Detected NUMA nodes: 1
EAL: RTE Version: 'DPDK 21.11.9'
EAL: Detected shared linkage of DPDK
```

**❌ testpmd 启动错误：**
```
$ dpdk-testpmd -v
EAL: Detected CPU lcores: 8
EAL: Detected NUMA nodes: 1
EAL: RTE Version: 'DPDK 21.11.9'
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /run/user/1000/dpdk/rte/mp_socket
EAL: Physical addresses are unavailable, selecting IOVA as VA mode.
EAL: Selected IOVA mode 'VA'
EAL: No free 2048 kB hugepages reported on node 0
EAL: No available 2048 kB hugepages reported
EAL: No available 1048576 kB hugepages reported
EAL: FATAL: Cannot get hugepage information.
EAL: Cannot get hugepage information.
EAL: Error - exiting with code: 1
  Cause: Cannot init EAL: Permission denied
```

**问题原因：**
- 大页未配置或已失效（reboot 后需要重新配置）
- `vfio-pci` 内核模块未加载
- `/dev/hugepages` 权限问题

**解决方法：**
```bash
# 1. 重新配置大页
sudo ./scripts/01_setup_hugepages.sh

# 2. 加载 vfio-pci 模块
sudo modprobe vfio-pci

# 3. 确认大页状态
cat /proc/meminfo | grep Huge
mount | grep huge
```

**网卡绑定状态：**
```
$ dpdk-devbind.py --status

Network devices using kernel driver
===================================
0000:02:01.0 '82545EM Gigabit Ethernet Controller (Copper) 100f' if=ens33 drv=e1000 unused= *Active*
0000:02:02.0 '82545EM Gigabit Ethernet Controller (Copper) 100f' if=ens34 drv=e1000 unused=
0000:0b:00.0 'VMXNET3 Ethernet Controller 07b0' if=ens192 drv=vmxnet3 unused= *Active*

No 'Baseband' devices detected
No 'Crypto' devices detected
No 'DMA' devices detected
No 'Eventdev' devices detected
No 'Mempool' devices detected
No 'Compress' devices detected
No 'Misc (rawdev)' devices detected
No 'Regex' devices detected
```

> 当前状态：
> - `ens192` (0000:0b:00.0) → `vmxnet3` 驱动（kernel 驱动，待绑定到 vfio-pci）
> - `ens33` (0000:02:01.0) → `e1000` 驱动（管理口）
> - `ens34` (0000:02:02.0) → `e1000` 驱动（备用）
>
> ⚠️ 注意：`testpmd` 命令不存在，正确命令是 `dpdk-testpmd`

### 原始记录文件

- 记录目录：`records/20260426_212251-vmxnet3-testpmd/`
- 环境检查结果：`ENV_CHECK.txt`

---

## 步骤 2：大页配置（01_setup_hugepages.sh）

**执行时间**：2026-04-27 21:33:00
**执行结果**：✅ 通过

**执行命令**：
```bash
sudo ./scripts/01_setup_hugepages.sh
```

**配置前后对比**：

| 指标 | 配置前 | 配置后 |
|------|--------|--------|
| HugePages_Total | 0 | 1024 |
| HugePages_Free | 0 | 1024 |
| Hugepagesize | 2048 kB | 2048 kB |
| Hugetlb | 0 kB | 2097152 kB (2GB) |

**配置后大页状态**：
```
AnonHugePages:         0 kB
ShmemHugePages:        0 kB
FileHugePages:         0 kB
HugePages_Total:    1024
HugePages_Free:     1024
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:       2048 kB
Hugetlb:         2097152 kB

hugetlbfs on /dev/hugepages type hugetlbfs (rw,relatime,pagesize=2M)
nodev on /mnt/huge type hugetlbfs (rw,relatime,pagesize=2M)
```

> ✅ 大页配置成功：1024 × 2MB = 2GB

**原始记录文件**：
- 记录目录：`records/20260426_212251-vmxnet3-testpmd/`
- 大页配置结果：`HUGEPAGE_SETUP.txt`

---

## 步骤 3：绑定 VMXNET3 到 vfio-pci（02_bind_vmxnet3.sh）

**执行时间**：2026-04-27 21:38:00
**执行结果**：❌ 失败

**执行命令**：
```bash
sudo DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind
```

**❌ 错误信息**：
```
ERROR: dpdk-devbind failed, rc=1. See .../BIND_AFTER.txt

# BIND_AFTER.txt 内容：
Error: bind failed for 0000:0b:00.0 - Cannot bind to driver vfio-pci: [Errno 22] Invalid argument
Error: unbind failed for 0000:0b:00.0 - Cannot open /sys/bus/pci/drivers//unbind: [Errno 13] Permission denied: '/sys/bus/pci/drivers//unbind'
```

**问题分析**：

| 检查项 | 结果 |
|--------|------|
| `/sys/kernel/iommu_groups/` | 目录存在但为空 |
| `vfio-pci` 驱动 | 已加载 |
| `/sys/bus/pci/drivers/vfio-pci/` | 存在且可访问 |
| IOMMU | ❌ **未启用** |

**根本原因**：`vfio-pci` 需要硬件 IOMMU（Intel VT-d 或 AMD-Vi）支持，但 VMware 虚拟机环境未启用 IOMMU 透传。

**解决方案**：

### 方案一：启用 VMware IOMMU（推荐）

在宿主机的 VMX 文件中添加：
```
vhv.enable = "TRUE"
hypervisor.cpuid.v0 = "FALSE"
```
或使用 VMware vSphere/vCenter 为 VM 启用 "PCI Passthrough"。

**VMX 配置文件分析**（`D:\software\install\VMware\Ubuntu22\vm\Ubuntu22-wq.vmx`）：
- 第 58 行已有 `vhv.enable = "TRUE"`
- 但 VMware **Workstation/Player 不支持将物理 IOMMU 透传给虚拟机**
- 这是 VMware 虚拟化平台的限制，而非配置问题
- **结论**：方案一在 VMware Workstation 环境下**不可行**

> **专家分析**：VMware Workstation 是 Type-2 Hypervisor，运行在宿主机操作系统之上。它虽然能给虚拟机提供虚拟化加速（vhv.enable），但不会把主板上的物理 IOMMU（AMD-Vi）暴露给客户机。Ubuntu 启动时即使用了 `iommu=on`，也找不到硬件支持。
>
> `vhv.enable` 的真正作用：只是让 VM 支持硬件虚拟化指令（VT-x/AMD-V），用于 CPU 虚拟化加速，**不是** IOMMU 透传。
>
> `vfio-pci` 依赖 IOMMU 的 DMA 重映射来实现用户空间设备访问。没有 IOMMU，内核无法安全地将 PCI 设备交给用户空间驱动。
>
> ℹ️ 如果使用 **VMware vSphere/ESXi** 环境（Type-1 Hypervisor，直接运行在硬件上），则可以通过 vSphere Client 为 VM 配置 PCI Passthrough（需要物理服务器 BIOS 启用 Intel VT-d 或 AMD-Vi）。

### 方案二：使用 uio_pci_generic（当前可行方案）

**操作步骤**：

```bash
# 1. 加载 uio_pci_generic 驱动
sudo modprobe uio_pci_generic

# 2. 绑定 VMXNET3 到 uio_pci_generic
sudo DPDK_DRIVER=uio_pci_generic DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind
```

**执行结果**：⏳ 待执行

**其他 AI 解释的专家评审**：

| 项目 | 其他 AI 说法 | 专家评审 |
|------|-------------|---------|
| uio_pci_generic | 推荐使用，正确 | ✅ 正确 |
| GRUB 配置 iommu=on | 建议去掉 | ❌ 没必要，你的 VM 本来就没有 IOMMU，设不设都没影响 |
| igb_uio | 需要编译 | ⚠️ uio_pci_generic 内置无需编译，更好 |
| PCI 地址示例 | 02:01.0 | ❌ 错误，这是 ens33（管理口），绑错会断 SSH！ |
| hugepages | 写入 GRUB | ⚠️ 重启后失效，建议写 systemd service |
| 多网卡 vmxnet3 | 建议添加 | ✅ 正确，vmxnet3 性能优于 e1000 |

**关键注意事项**：
- 你的 DPDK 网卡 PCI 是 `0000:0b:00.0`（ens192），**不是** 02:01.0
- 绑到 02:01.0 会断 SSH！

**查看 PCI 地址方法**：
```bash
$ ethtool -i ens192
driver: vmxnet3
version: 1.7.0.0-k-NAPI
firmware-version:
expansion-rom-version:
bus-info: 0000:0b:00.0
supports-statistics: yes
supports-test: no
supports-eeprom-access: no
supports-register-dump: yes
supports-priv-flags: no
```

> ✅ `bus-info: 0000:0b:00.0` 就是 ens192 对应的 PCI 地址

**加载 uio_pci_generic 驱动**：
```bash
$ sudo modprobe uio_pci_generic
[sudo] password for wq7:
```

**验证驱动加载**：
```bash
$ lsmod | grep uio_pci_generic
uio_pci_generic        12288  0
uio                    28672  1 uio_pci_generic
```
> ✅ `uio_pci_generic` 和 `uio` 模块已成功加载

**执行绑定命令**：
```bash
$ sudo DPDK_DRIVER=uio_pci_generic DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind
[OK] Bind finished:
  before: .../BIND_BEFORE.txt
  after : .../BIND_AFTER.txt
```

**绑定结果**：

```
Network devices using DPDK-compatible driver
============================================
0000:0b:00.0 'VMXNET3 Ethernet Controller 07b0' drv=uio_pci_generic unused=vmxnet3,vfio-pci

Network devices using kernel driver
===================================
0000:02:01.0 '82545EM Gigabit Ethernet Controller (Copper) 100f' if=ens33 drv=e1000 unused=vfio-pci,uio_pci_generic *Active*
0000:02:02.0 '82545EM Gigabit Ethernet Controller (Copper) 100f' if=ens34 drv=e1000 unused=vfio-pci,uio_pci_generic
```

> ✅ **绑定成功**：`0000:0b:00.0` (ens192) 已绑定到 `uio_pci_generic` 驱动
> - `ens33` (管理口) 未受影响，继续使用 `e1000` 驱动
> - SSH 连接正常

**绑定成功验证（四种方法）**：

| 验证方法 | 绑定前（kernel） | 绑定后（DPDK） |
|---------|-----------------|----------------|
| `dpdk-devbind.py --status` | ens192 在 kernel 驱动列表 | ✅ 在 DPDK 驱动列表：`drv=uio_pci_generic` |
| `ip link show ens192` | ✅ 显示 ens192 | ❌ Device "ens192" does not exist |
| `ifconfig` | ✅ 显示 ens192 | ❌ 看不到 ens192 |
| `lspci -k -s 0000:0b:00.0` | `drv=vmxnet3` | ✅ `drv=uio_pci_generic` |

**验证过程**：

```bash
$ dpdk-devbind.py --status
Network devices using DPDK-compatible driver
============================================
0000:0b:00.0 'VMXNET3 Ethernet Controller 07b0' drv=uio_pci_generic unused=vmxnet3,vfio-pci
```

```bash
$ ip link show ens192
Device "ens192" does not exist.
```

```bash
$ ifconfig
ens33: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
        inet 192.168.65.135  netmask 255.255.255.0
        ether 00:0c:29:f8:f6:6e  txqueuelen 1000  (Ethernet)
        ...
ens34: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
        ether 00:0c:29:f8:f6:78
        ...
lo: flags=73<UP,LOOPBACK,RUNNING>  mtu 65536
        inet 127.0.0.1  netmask 255.0.0.0
        ...
# ens192 已消失
```

```bash
$ lspci -k -s 0000:0b:00.0
0b:00.0 Ethernet controller: VMware VMXNET3 Ethernet Controller (rev 01)
        DeviceName: Ethernet2
        Subsystem: VMware VMXNET3 Ethernet Controller
        Kernel driver in use: uio_pci_generic
        Kernel modules: vmxnet3
```

```bash
$ ls -l /sys/bus/pci/drivers/uio_pci_generic/0000:0b:00.0/
total 0
-r--r--r-- 1 root root  4096  4月 27 22:01 driver -> ../../../../bus/pci/drivers/uio_pci_generic
-rw------- 1 root root 65536  4月 27 22:01 resource0    # BAR0 MMIO
-rw------- 1 root root  8192  4月 27 22:01 resource2    # BAR2
drwxr-xr-x 1 root root     0  4月 27 22:01 uio             # UIO 映射目录
...
```

> **为什么 ens192 从 ip link 消失？**
> 当网卡被 DPDK (uio_pci_generic) 接管后，网卡从 Linux 标准网络栈脱离，由 DPDK 用户空间驱动管理。所以 `ip link` 和 `ifconfig` 看不到了 —— 这是正常现象，不是故障。

**DPDK 环境诊断要点**：

| 场景 | 用什么命令 |
|------|-----------|
| 常规 Linux 环境 | `ifconfig` / `ip addr` |
| DPDK 环境 | `dpdk-devbind.py --status` |

**标准发现步骤**：
```bash
# 1. 查看所有 Ethernet 设备
lspci | grep -i ethernet

# 2. 查看每个设备的驱动情况（DPDK 标准诊断命令）
dpdk-devbind.py --status

# 3. 如果看到某个 PCI 在 DPDK 列表，就是 DPDK 设备
```

> **关键点**：在 DPDK 环境下，`ifconfig` 只显示 kernel 网络栈的设备，DPDK 设备需要用 `dpdk-devbind.py` 查看。任何 DPDK 环境，第一步都应该跑 `dpdk-devbind.py --status`。

**原始记录文件**：
- 记录目录：`records/20260426_212251-vmxnet3-testpmd/`
- `BIND_BEFORE.txt`
- `BIND_AFTER.txt`

---

## 步骤 4：运行 testpmd 冒烟测试（03_run_testpmd.sh）

> ⚠️ **重启后需重新执行（重要）**：
>
> | 配置项 | 重启后状态 | 重新执行 |
> |--------|-----------|---------|
> | 大页配置 | ❌ 丢失 | `sudo ./scripts/01_setup_hugepages.sh` |
> | 网卡绑定 | ❌ 恢复到 vmxnet3 | `sudo modprobe uio_pci_generic && sudo DPDK_DRIVER=uio_pci_generic DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind` |

**执行前检测**：每次运行 testpmd 前，**必须确认大页已配置**：

```bash
# 检查大页状态
cat /proc/meminfo | grep Huge
```

**当前大页状态**（执行 `sudo ./scripts/01_setup_hugepages.sh` 后）：
```
AnonHugePages:         0 kB
ShmemHugePages:        0 kB
FileHugePages:         0 kB
HugePages_Total:    1024
HugePages_Free:     1024
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:       2048 kB
Hugetlb:         2097152 kB
```

> ✅ 大页已配置：1024 × 2MB = 2GB

**执行命令**：
```bash
sudo ./scripts/03_run_testpmd.sh
```

**终端输出**：
```
wq7@wq7-virtual-machine:~/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd$ sudo ./scripts/03_run_testpmd.sh

[OK] testpmd smoke log saved:
/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd/records/20260426_212251-vmxnet3-testpmd/TESTPMD.log

Next:
  ./scripts/04_collect_stats.sh
```

**执行结果**：✅ 成功

**完整日志**：`/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd/records/20260426_212251-vmxnet3-testpmd/TESTPMD.log`

**关键输出**：

```
EAL: Selected IOVA mode 'PA'
EAL: VFIO support initialized
EAL: Probe PCI driver: net_vmxnet3 (15ad:07b0) device: 0000:0b:00.0 (socket 0)
testpmd: create a new mbuf pool <mb_pool_0>: n=155456, size=2176, socket=0
Set io packet forwarding mode
Auto-start selected
Configuring Port 0 (socket 0)
Port 0: 00:0C:29:F8:F6:82
Checking link statuses...
Done
io packet forwarding - ports=1 - cores=1 - streams=1
Logical Core 1 (socket 0) forwards packets on 1 streams:
  RX P=0/Q=0 (socket 0) -> TX P=0/Q=0 (socket 0)
```

**Port 0 统计**（运行 20 秒，无流量）：

```
RX-packets: 0          RX-missed: 0          RX-bytes:  0
TX-packets: 0          TX-errors: 0          TX-bytes:  0
```

**说明**：
- `rc=124` 表示 timeout 正常停止 testpmd，属于预期行为
- `RX-packets: 0` 是因为 ens192 网卡未连接任何设备，没有实际流量
- ✅ **testpmd 成功启动并运行**，冒烟测试通过

---

## 步骤 5：收集统计信息（04_collect_stats.sh）

**执行命令**：
```bash
./scripts/04_collect_stats.sh
```

**终端输出**：
```
wq7@wq7-virtual-machine:~/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd$ ./scripts/04_collect_stats.sh

[OK] Stats collected in:
/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd/records/20260426_212251-vmxnet3-testpmd

Next:
  ./scripts/05_make_review_bundle.sh
```

**执行结果**：✅ 成功

**收集的统计文件**：

| 文件 | 说明 |
|------|------|
| `ENV_CHECK.txt` | 初始环境检查 |
| `HUGEPAGE_STATUS.txt` | 当前大页状态 |
| `HUGEPAGE_SETUP.txt` | 大页配置记录 |
| `BIND_BEFORE.txt` | 绑定前状态 |
| `BIND_AFTER.txt` | 绑定后状态 |
| `BIND_STATUS.txt` | 当前绑定状态 |
| `PCI_DETAIL.txt` | PCI 设备详情 |
| `IP_LINK.txt` | IP link 状态 |
| `IP_ADDR.txt` | IP 地址 |
| `ETHTOOL_STATS.txt` | ethtool 统计 |
| `ETHTOOL_DRIVER.txt` | ethtool 驱动信息 |
| `DMESG_DPDK_NET.txt` | dmesg DPDK 网络日志 |
| `TESTPMD.log` | testpmd 运行日志 |
| `COMMANDS.md` | 执行的命令记录 |

**当前大页状态**：
```
HugePages_Total:    1024
HugePages_Free:     1019   # testpmd 使用了 5 个大页
Hugepagesize:       2048 kB
```

**PCI 设备详情**：
```
0b:00.0 Ethernet controller: VMware VMXNET3 Ethernet Controller (rev 01)
        DeviceName: Ethernet2
        Subsystem: VMware VMXNET3 Ethernet Controller
        Physical Slot: 192
        Region 0: Memory at fd3fc000 (32-bit, non-prefetchable) [size=4K]
        Region 1: Memory at fd3fd000 (32-bit, non-prefetchable) [size=4K]
        Region 2: Memory at fd3fe000 (32-bit, non-prefetchable) [size=8K]
        Region 3: I/O ports at 4000 [size=16]
        Kernel driver in use: uio_pci_generic
```

> ✅ 统计收集完成，所有记录已保存

---

## 步骤 6：生成 review bundle（05_make_review_bundle.sh）

**执行命令**：
```bash
./scripts/05_make_review_bundle.sh
```

**终端输出**：
```
wq7@wq7-virtual-machine:~/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd$ ./scripts/05_make_review_bundle.sh
[OK] Review bundle generated:
  /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd/records/20260426_212251-vmxnet3-testpmd/REVIEW_BUNDLE.md
  /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd/reports/lab-vmxnet3-testpmd_exec_board.md
```

**执行结果**：✅ 成功

**生成的报告**：
- `records/20260426_212251-vmxnet3-testpmd/REVIEW_BUNDLE.md` - 实验记录汇总
- `reports/lab-vmxnet3-testpmd_exec_board.md` - 执行看板

> ✅ **lab-vmxnet3-testpmd 实验全部完成！**
