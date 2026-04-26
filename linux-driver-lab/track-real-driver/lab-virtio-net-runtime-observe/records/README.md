# records

这里存放真实运行期实验记录。

## 2026-04-25 QEMU virtio-net 测试记录

### 测试环境

| 项目 | 值 |
|------|-----|
| 测试机器 | 192.168.65.135 (VMware + QEMU) |
| Kernel | 5.15.10 #2 SMP (已启用 VIRTIO_PCI=y, VIRTIO_NET=y) |
| 网卡 | virtio-net-pci (QEMU user-mode 网络) |
| MAC | 52:54:00:12:34:56 |
| Guest IP | 10.0.2.15 |
| Host 网关 | 10.0.2.2 |
| tracefs | /sys/kernel/tracing |

### 测试过程

#### 1. 启动 QEMU (virtio-net 环境)

```bash
# 在测试机器 192.168.65.135 上执行
qemu-system-x86_64 \
  -kernel /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/output/x86/bzImage \
  -initrd /tmp/lab_rootfs.img \
  -nographic \
  -append 'console=ttyS0 noapic root=/dev/ram0 rw rdinit=/init' \
  -netdev user,id=net0,hostfwd=tcp::5555-:22 \
  -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56 \
  -serial telnet:127.0.0.1:5557,server,nowait \
  -monitor telnet:127.0.0.1:5556,server,nowait
```

#### 2. Guest 内配置网络

```bash
busybox ifconfig eth0 up
busybox ifconfig eth0 10.0.2.15 netmask 255.255.255.0
mount -t tracefs nodev /sys/kernel/tracing
```

#### 3. Idle Baseline 测试

```bash
# 创建记录目录
mkdir -p /lab/records/20260425_183500-idle-baseline

# 记录初始统计
cat /sys/class/net/eth0/statistics/rx_packets > /lab/records/20260425_183500-idle-baseline/rx_start.txt

# 启用 trace
echo 0 > /sys/kernel/tracing/tracing_on
echo nop > /sys/kernel/tracing/current_tracer
> /sys/kernel/tracing/trace
echo 1 > /sys/kernel/tracing/events/net/netif_receive_skb/enable
echo 1 > /sys/kernel/tracing/tracing_on

# 等待约 3 秒
sleep 3

# 停止 trace
echo 0 > /sys/kernel/tracing/tracing_on

# 记录最终统计
cat /sys/class/net/eth0/statistics/rx_packets > /lab/records/20260425_183500-idle-baseline/rx_end.txt

# 保存 trace
cat /sys/kernel/tracing/trace > /lab/records/20260425_183500-idle-baseline/trace.txt
```

#### 4. Ping Workload 测试

```bash
# 创建记录目录
mkdir -p /lab/records/20260425_184000-ping-workload

# 记录初始统计
cat /sys/class/net/eth0/statistics/rx_packets > /lab/records/20260425_184000-ping-workload/rx_start.txt

# 启用 trace
echo 0 > /sys/kernel/tracing/tracing_on
> /sys/kernel/tracing/trace
echo 1 > /sys/kernel/tracing/events/net/netif_receive_skb/enable
echo 1 > /sys/kernel/tracing/tracing_on

# 执行 ping
busybox ping -c 20 10.0.2.2

# 停止 trace
echo 0 > /sys/kernel/tracing/tracing_on

# 记录最终统计
cat /sys/class/net/eth0/statistics/rx_packets > /lab/records/20260425_184000-ping-workload/rx_end.txt

# 保存 trace
cat /sys/kernel/tracing/trace > /lab/records/20260425_184000-ping-workload/trace.txt
```

### 测试结果

#### Idle Baseline (20260425_183500-idle-baseline)

| 指标 | 值 |
|------|-----|
| RX packets start | 1 |
| RX packets end | 7 |
| RX delta | +6 |
| trace events | 6 x netif_receive_skb |

#### Ping Workload (20260425_184000-ping-workload)

| 指标 | 值 |
|------|-----|
| RX packets start | 7 |
| RX packets end | 28 |
| RX delta | +21 |
| Ping 发送 | 20 |
| Ping 接收 | 20 |
| 丢包率 | 0% |
| RTT min/avg/max | 0.394/0.593/1.031 ms |
| trace events | 21 x netif_receive_skb |

### 验证方法

```bash
# 1. 检查 trace 内容
cat /lab/records/20260425_184000-ping-workload/trace.txt

# 输出应包含类似:
# busybox-102  [000] ..s.. 196.090753: netif_receive_skb: dev=eth0 skbaddr=ffff99ac411bf500 len=84

# 2. 检查统计变化
cat /lab/records/20260425_184000-ping-workload/rx_start.txt  # 7
cat /lab/records/20260425_184000-ping-workload/rx_end.txt    # 28

# 3. 计算增量
echo $(( $(cat rx_end.txt) - $(cat rx_start.txt) ))  # 21
```

### trace point 含义

- `netif_receive_skb`: skb 已进入网络协议栈，在送往上层之前
- `dev=eth0`: 入方向网络设备
- `skbaddr`: skb 缓冲区地址，可用于追踪生命周期
- `len`: 数据包长度 (84 = 64字节 ICMP echo + 20字节 IP头)

### 如何复现

1. 在测试机器编译支持 virtio 的 kernel (CONFIG_VIRTIO_PCI=y, CONFIG_VIRTIO_NET=y)
2. 准备包含 busybox 的 minimal rootfs
3. 使用上述 QEMU 命令启动
4. 按测试过程执行

### 限制

- QEMU user-mode 网络不支持广播/多播
- 无 ethtool/iptables 等工具，需用 busybox 替代
- trace buffer 有限，长时间测试需适时保存