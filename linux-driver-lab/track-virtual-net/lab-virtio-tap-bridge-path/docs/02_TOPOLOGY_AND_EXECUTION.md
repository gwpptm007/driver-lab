# 02_TOPOLOGY_AND_EXECUTION

## 推荐拓扑

```
guest:
  eth0 = 192.168.100.2/24

host:
  br-vnet0 = 192.168.100.1/24
  tap-vnet0 attached to br-vnet0
```

## QEMU 关键参数

```
-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no
-device virtio-net-pci,netdev=net0
```

## 执行步骤

### Step 1：环境检查

```bash
./scripts/check_env.sh
```

### Step 2：创建 bridge + tap

```bash
echo wq123456! | sudo -S ip link add name br-vnet0 type bridge
echo wq123456! | sudo -S ip addr add 192.168.100.1/24 dev br-vnet0
echo wq123456! | sudo -S ip link set br-vnet0 up
echo wq123456! | sudo -S modprobe tun
echo wq123456! | sudo -S ip tuntap add dev tap-vnet0 mode tap user $(whoami)
echo wq123456! | sudo -S ip link set tap-vnet0 master br-vnet0
echo wq123456! | sudo -S ip link set tap-vnet0 up
```

### Step 3：启动 guest

把 QEMU 网络参数接入启动脚本：

```
-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no
-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56
```

### Step 4：guest 内配 IP 并 ping

```bash
ip addr add 192.168.100.2/24 dev eth0
ip link set eth0 up
ping -c 5 192.168.100.1
```

### Step 5：采集 host 状态

```bash
ip link show tap-vnet0
ip link show br-vnet0
bridge link
bridge fdb show
lsmod | grep -E 'vhost|tun|bridge'
tcpdump -i tap-vnet0 -n
tcpdump -i br-vnet0 -n
```

## 观测点

- `ip link show tap-vnet0`: 确认 tap 已 UP
- `bridge link`: 确认 tap 已加入 bridge
- `bridge fdb show`: 确认 FDB 学到 guest MAC
- `tcpdump`: 在 tap/bridge 上能看到 ICMP 包