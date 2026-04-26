# 00_ENVIRONMENT_PREPARE

> VMXNET3 网卡配置 - 从 e1000 改为 VMXNET3

## 背景

在 VMware 虚拟机中，默认的网络适配器通常是 `e1000`（Intel e1000 模拟），但 DPDK 推荐使用 `VMXNET3` 以获得更好的性能：
- 更少的 CPU 开销
- 支持 multi-queue
- 更好的 checksum offload 支持
- 更适合 DPDK poll mode 场景

## 测试机环境信息

### 主机信息

- **VM 位置**：`D:\software\install\VMware\Ubuntu22\`
- **VM 名称**：Ubuntu22-wq
- **VMX 配置文件**：`vm/Ubuntu22-wq.vmx`
- **Guest OS**：Ubuntu 22.04.5 Desktop (Linux 6.8.0-110-generic)
- **测试机 IP**：192.168.65.135（NAT）
- **用户**：wq7
- **密码**：wq123456!

### ifconfig 输出（当前状态）

```
ens33: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
        inet 192.168.65.135  netmask 255.255.255.0  broadcast 192.168.65.255
        inet6 fe80::6e4d:b76b:3e79:cf8d  prefixlen 64  scopeid 0x20<link>
        ether 00:0c:29:f8:f6:6e  txqueuelen 1000  (Ethernet)
        RX packets 243  bytes 37594 (37.5 KB)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 250  bytes 43067 (43.0 KB)
        TX errors 0  dropped 0  overruns 0  carrier 0  collisions 0

ens34: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
        ether 00:0c:29:f8:f6:78  txqueuelen 1000  (Ethernet)
        RX packets 0  bytes 0 (0.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 94  bytes 15299 (15.2 KB)
        TX errors 0  dropped 0  overruns 0  carrier 0  collisions 0

ens192: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
        inet 192.168.100.1  netmask 255.255.255.0  broadcast 192.168.100.255
        inet6 fe80::20c:29ff:fef8:f682  prefixlen 64  scopeid 0x20<link>
        ether 00:0c:29:f8:f6:82  txqueuelen 1000  (Ethernet)
        RX packets 0  bytes 0 (0.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 76  bytes 9437 (9.4 KB)
        TX errors 0  dropped 0  overruns 0  carrier 0  collisions 0

lo: flags=73<UP,LOOPBACK,RUNNING>  mtu 65536
        inet 127.0.0.1  netmask 255.0.0.0
        inet6 ::1  prefixlen 128  scopeid 0x10<host>
        loop  txqueuelen 1000  (Local Loopback)
        RX packets 243  bytes 28492 (28.4 KB)
        TX packets 243  bytes 28492 (28.4 KB)
```

### ethtool 输出（当前状态）

```bash
# ens192 (VMXNET3 - DPDK 用)
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

# ens33 (e1000 - NAT 管理)
$ ethtool -i ens33
driver: e1000
version: 6.8.0-110-generic
firmware-version:
expansion-rom-version:
bus-info: 0000:02:01.0
supports-statistics: yes
supports-test: yes
supports-eeprom-access: yes
supports-register-dump: yes
supports-priv-flags: no

# ens34 (e1000 - hostonly 备用)
$ ethtool -i ens34
driver: e1000
version: 6.8.0-110-generic
firmware-version:
expansion-rom-version:
bus-info: 0000:02:02.0
supports-statistics: yes
supports-test: yes
supports-eeprom-access: yes
supports-register-dump: yes
supports-priv-flags: no
```

### lspci 输出（当前状态）

```
02:01.0 Ethernet controller: Intel Corporation 82545EM Gigabit Ethernet Controller (Copper) (rev 01)
02:02.0 Ethernet controller: Intel Corporation 82545EM Gigabit Ethernet Controller (Copper) (rev 01)
0b:00.0 Ethernet controller: VMware VMXNET3 Ethernet Controller (rev 01)
```

### netplan 配置（永久 IP）

```bash
$ cat /etc/netplan/ens192.yaml
network:
  version: 2
  ethernets:
    ens192:
      addresses:
        - 192.168.100.1/24
```

### 网卡接口列表

```
ens192  (VMXNET3, PCI 0b:00.0, IP 192.168.100.1/24) ← DPDK 测试用
ens33   (e1000,    PCI 02:01.0, IP 192.168.65.135/24) ← SSH 管理
ens34   (e1000,    PCI 02:02.0, 无 IP) ← 备用
lo      (loopback, 127.0.0.1)
```

---

## 修改前的状态

### VMX 配置（修改前）

```
ethernet0.connectionType = "nat"
ethernet0.addressType = "generated"
ethernet0.virtualDev = "e1000"
ethernet0.present = "TRUE"

ethernet1.addressType = "generated"
ethernet1.virtualDev = "e1000"
ethernet1.connectionType = "hostonly"
ethernet1.present = "TRUE"

ethernet2.connectionType = "custom"
ethernet2.addressType = "generated"
ethernet2.virtualDev = "e1000"
```

### Guest 内网卡（修改前）

| 接口 | 驱动 | 类型 | 用途 |
|------|------|------|------|
| ens33 | e1000 | NAT | SSH 连接（192.168.65.135） |
| ens34 | e1000 | hostonly | - |
| ens35 | e1000 | VMnet3 (custom) | - |

### PCI 信息（修改前）

```
02:01.0 Ethernet controller: Intel Corporation 82545EM Gigabit Ethernet Controller (Copper) (rev 01)
02:02.0 Ethernet controller: Intel Corporation 82545EM Gigabit Ethernet Controller (Copper) (rev 01)
02:03.0 Ethernet controller: Intel Corporation 82545EM Gigabit Ethernet Controller (Copper) (rev 01)
```

---

## 修改步骤

### 步骤 1：关闭虚拟机

**重要**：必须完全关闭虚拟机（power off），不能只是暂停（suspend）。

在 VMware Workstation 中：
- 虚拟机 → 电源 → 关闭客户机

或通过 SSH 在 guest 内：
```bash
sudo shutdown -h now
```

### 步骤 2：编辑 VMX 配置文件

找到 VMX 文件路径：
```
D:\software\install\VMware\Ubuntu22\vm\Ubuntu22-wq.vmx
```

**备份**（推荐）：
```bash
cp Ubuntu22-wq.vmx Ubuntu22-wq.vmx.bak
```

### 步骤 3：修改 ethernet2 为 VMXNET3

找到 `ethernet2` 相关配置行：

**修改前**：
```
ethernet2.connectionType = "custom"
ethernet2.addressType = "generated"
ethernet2.virtualDev = "e1000"
```

**修改后**：
```
ethernet2.connectionType = "custom"
ethernet2.virtualDev = "vmxnet3"
```

### 步骤 4：重新启动虚拟机

启动 VMware 虚拟机。

首次启动时，VMware 可能会弹出提示"检测到硬件配置更改"，选择"我已移动该虚拟机"或"我已复制该虚拟机"。

### 步骤 5：验证 Guest 内网卡变化

```bash
# 查看所有网卡
ip -br link

# 查看 vmxnet3 详细信息
ethtool -i ens192
```

---

## 修改后的状态

### VMX 配置（修改后）

```
ethernet2.connectionType = "custom"
ethernet2.virtualDev = "vmxnet3"
ethernet2.present = "TRUE"
ethernet2.addressType = "generated"
ethernet2.generatedAddress = "00:0c:29:f8:f6:82"
ethernet2.vnet = "VMnet3"
ethernet2.displayName = "VMnet3"
ethernet2.pciSlotNumber = "192"
```

**注意**：ens35 变成了 **ens192**，这是因为 VMXNET3 使用不同的 PCI 位置（从 02:03.0 变为 0b:00.0）。

---

## 添加 IP 地址

VMXNET3 默认没有 IPv4 地址，需要手动添加。

### 临时添加（重启后失效）

```bash
sudo ip addr add 192.168.100.1/24 dev ens192
ip -br addr show ens192
```

输出：
```
ens192           UP             192.168.100.1/24 fe80::b107:8145:d181:abe2/64
```

### 永久配置（Ubuntu netplan）

**方法 1：手动编辑**

```bash
# 创建 netplan 配置文件
cat > /tmp/ens192.yaml << 'EOF'
network:
  version: 2
  ethernets:
    ens192:
      addresses:
        - 192.168.100.1/24
EOF

# 复制到 netplan 目录
sudo cp /tmp/ens192.yaml /etc/netplan/ens192.yaml

# 设置正确权限（netplan 要求 600）
sudo chmod 600 /etc/netplan/ens192.yaml

# 应用配置
sudo netplan generate
sudo netplan apply
```

**方法 2：通过 SCP 传输配置文件（本文档使用的方法）**

1. 在本地创建 `ens192.yaml`：
```yaml
network:
  version: 2
  ethernets:
    ens192:
      addresses:
        - 192.168.100.1/24
```

2. 复制到测试机：
```bash
scp ens192.yaml wq7@192.168.65.135:/tmp/ens192.yaml
```

3. 在测试机上应用：
```bash
echo wq123456! | sudo -S cp /tmp/ens192.yaml /etc/netplan/ens192.yaml
echo wq123456! | sudo -S chmod 600 /etc/netplan/ens192.yaml
echo wq123456! | sudo -S netplan generate
echo wq123456! | sudo -S netplan apply
```

**验证**：
```bash
ip -br addr show ens192
```

输出应为：
```
ens192           UP             192.168.100.1/24 fe80::20c:29ff:fef8:f682/64
```

---

## 三张网卡最终状态

| 接口 | 驱动 | 类型 | IP 地址 | 用途 |
|------|------|------|---------|------|
| ens33 | e1000 | NAT | 192.168.65.135/24 | SSH 连接（不可改） |
| ens34 | e1000 | hostonly | 无 | 备用 |
| ens192 | vmxnet3 | VMnet3 | 192.168.100.1/24 | **DPDK 测试用** |

---

## 重启后验证

重启后 IP 持久化验证：

```bash
# 重启测试
ssh wq7@192.168.65.135 "sudo reboot"

# 等待重启（大约 30 秒）
sleep 30

# 重新连接并验证
ssh wq7@192.168.65.135 "ip -br addr show ens192"
```

预期输出：
```
ens192           UP             192.168.100.1/24 fe80::20c:29ff:fef8:f682/64
```

---

## 如果需要回退

如果 VMXNET3 有问题，想恢复为 e1000：

1. 关闭虚拟机
2. 编辑 `Ubuntu22-wq.vmx`，将 `ethernet2.virtualDev = "vmxnet3"` 改回 `ethernet2.virtualDev = "e1000"`
3. 重新启动虚拟机
4. 网卡会恢复为 ens35

---

## 注意事项

### 1. VMXNET3 需要 VMware Tools

确保 VMware Tools 已安装，否则 VMXNET3 可能无法正常工作。

```bash
# 检查 VMware Tools 状态
vmware-toolbox-cmd -v
```

输出示例：`12.3.5.46049 (build-22544099)`

### 2. VMXNET3 使用不同 IRQ 和队列

VMXNET3 默认支持多队列，这在 DPDK 中是优势。

```bash
# 查看网卡队列
cat /sys/class/net/ens192/queues/rx-0/rps_cpus
cat /sys/class/net/ens192/queues/tx-0/tx_queue
```

### 3. 网卡名称可能变化

从 e1000 切换到 VMXNET3 后，网卡名称可能从 ens35 变成 ens192（数字取决于 PCI 总线位置）。如果写了固定网卡名的脚本，需要更新。

### 4. 为什么选择 ethernet2 而不是 ethernet0/1

- **ethernet0 (ens33)**：用于 NAT 和 SSH 连接（192.168.65.135），不能改
- **ethernet1 (ens34)**：hostonly，备用
- **ethernet2 (ens35/ens192)**：VMnet3，改为 VMXNET3 用于 DPDK

### 5. PCI Slot Number 会变化

修改 VMXNET3 后，`ethernet2.pciSlotNumber` 从 `35` 变为 `192`，这是正常的（VMXNET3 使用不同的 PCI 位置）。

### 6. netplan 权限警告

执行 `netplan generate` 时可能出现权限警告：
```
WARNING: Permissions for /etc/netplan/ens192.yaml are too open. Netplan configuration should NOT be accessible by others.
```

这是建议性警告，不影响功能。如需消除：
```bash
sudo chmod 600 /etc/netplan/ens192.yaml
```

---

## 验证清单

修改完成后，在测试机上验证：

- [ ] `ethtool -i ens192` 显示 `driver: vmxnet3`
- [ ] `lspci | grep VMXNET3` 显示 `0b:00.0`
- [ ] `ip -br addr show ens192` 显示 `192.168.100.1/24`
- [ ] `cat /etc/netplan/ens192.yaml` 显示正确配置
- [ ] 重启后 IP 仍然存在（持久化验证）
- [ ] 可以 `ping 192.168.100.1`（自检）
- [ ] 可以通过 SSH 连接 192.168.65.135（管理网）

---

## 故障排除

### 问题：ens192 没有 IP 地址

```bash
# 检查网卡状态
ip -br link show ens192
ip -br addr show ens192

# 如果是 DOWN，启用它
sudo ip link set ens192 up

# 检查 netplan 配置
cat /etc/netplan/ens192.yaml

# 重新应用 netplan
sudo netplan generate
sudo netplan apply
```

### 问题：VMXNET3 驱动加载失败

```bash
# 检查模块是否加载
lsmod | grep vmxnet

# 如果没有，手动加载
sudo modprobe vmxnet3

# 检查 dmesg 错误
dmesg | grep -i vmxnet
```

### 问题：netplan apply 报错

```bash
# 检查 YAML 语法
sudo netplan generate --debug

# 查看所有 netplan 配置
ls -la /etc/netplan/
cat /etc/netplan/*.yaml
```