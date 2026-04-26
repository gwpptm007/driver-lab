# 02_TOPOLOGY_AND_EXECUTION

## 拓扑

```
guest eth0(virtio-net-pci): 192.168.100.2/24
host br-vnet0:              192.168.100.1/24
host tap-vnet0:             attached to br-vnet0
```

### vhost=off 参数

```
-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=off
-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56
```

### vhost=on 参数

```
-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=on
-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56
```

## 执行步骤

### Step 1：检查 vhost 环境

```bash
lsmod | grep vhost
ls -l /dev/vhost-net
sudo modprobe vhost_net
```

### Step 2：创建 bridge + tap（复用 lab-virtio-tap-bridge-path 基础）

```bash
echo wq123456! | sudo -S ip link add name br-vnet0 type bridge
echo wq123456! | sudo -S ip addr add 192.168.100.1/24 dev br-vnet0
echo wq123456! | sudo -S ip link set br-vnet0 up
echo wq123456! | sudo -S ip tuntap add dev tap-vnet0 mode tap user $(whoami)
echo wq123456! | sudo -S ip link set tap-vnet0 master br-vnet0
echo wq123456! | sudo -S ip link set tap-vnet0 up
```

### Step 3：vhost=off 测试

启动 guest（vhost=off），guest 内：

```bash
ip addr add 192.168.100.2/24 dev eth0
ip link set eth0 up
ping -c 5 192.168.100.1
```

host 采集：

```bash
ip -br link
bridge link
bridge fdb show
lsmod | grep -E 'vhost|tun|bridge'
```

### Step 4：vhost=on 测试

重启 guest 或重新启动 QEMU，用 `vhost=on` 再跑同样 workload。

host 采集同样状态。

### Step 5：对照

对比 vhost=off 和 vhost=on 的：
- RTT 结果
- `lsmod | grep vhost` 是否显示 vhost_net 被使用
- QEMU 参数差异

## 观测点

- `/dev/vhost-net`: vhost=on 时 QEMU 会通过 ioctl 传入 virtqueue 映射
- `lsmod | grep vhost`: vhost=on 时 vhost_net 模块处于活跃状态
- `bridge fdb show`: 两种模式下 FDB 学习行为相同（路径差异在 backend 不在 bridge）

## 常见问题

### QEMU 报 vhost-net 不可用

```bash
ls -l /dev/vhost-net
lsmod | grep vhost
sudo modprobe vhost_net
```

### guest ping 不通 host bridge IP

先回退到 `lab-virtio-tap-bridge-path` 检查 tap/bridge 基础路径。

### vhost=on 和 off 看不出差异

先确保路径配置和 host 状态可解释，再考虑 iperf3/perf/ftrace。