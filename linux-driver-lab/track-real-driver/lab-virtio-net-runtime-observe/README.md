# lab-virtio-net-runtime-observe

> 把源码理解变成运行期证据

## 快速开始

### 1. 看懂目标
```
idle baseline  →  对照组
ping workload  →  小流量观测
iperf3         →  大流量观测
```
trace point 捕获 RX/TX 路径，与 source-dive 图对照。

### 2. 环境准备 (QEMU + virtio-net)

**Kernel 要求**：编译时启用 `CONFIG_VIRTIO_PCI=y`, `CONFIG_VIRTIO_NET=y`

**QEMU 启动命令**：
```bash
qemu-system-x86_64 \
  -kernel /path/to/bzImage \
  -initrd /path/to/lab_rootfs.img \
  -nographic \
  -append 'console=ttyS0 noapic root=/dev/ram0 rw rdinit=/init' \
  -netdev user,id=net0,hostfwd=tcp::5555-:22 \
  -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56 \
  -serial telnet:127.0.0.1:5557,server,nowait
```

**Guest 内配置**：
```bash
busybox ifconfig eth0 up
busybox ifconfig eth0 10.0.2.15 netmask 255.255.255.0
mount -t tracefs nodev /sys/kernel/tracing
```

### 3. 执行测试

```bash
# Idle baseline
mkdir -p /lab/records/20260425_183500-idle-baseline
cat /sys/class/net/eth0/statistics/rx_packets > /lab/records/20260425_183500-idle-baseline/rx_start.txt
echo 1 > /sys/kernel/tracing/events/net/netif_receive_skb/enable
echo 1 > /sys/kernel/tracing/tracing_on
sleep 3
echo 0 > /sys/kernel/tracing/tracing_on
cat /sys/class/net/eth0/statistics/rx_packets > /lab/records/20260425_183500-idle-baseline/rx_end.txt
cat /sys/kernel/tracing/trace > /lab/records/20260425_183500-idle-baseline/trace.txt

# Ping workload
mkdir -p /lab/records/20260425_184000-ping-workload
cat /sys/class/net/eth0/statistics/rx_packets > /lab/records/20260425_184000-ping-workload/rx_start.txt
echo 1 > /sys/kernel/tracing/tracing_on
busybox ping -c 20 10.0.2.2
echo 0 > /sys/kernel/tracing/tracing_on
cat /sys/class/net/eth0/statistics/rx_packets > /lab/records/20260425_184000-ping-workload/rx_end.txt
cat /sys/kernel/tracing/trace > /lab/records/20260425_184000-ping-workload/trace.txt
```

### 4. 验证结果

```bash
# 查看 trace
cat /lab/records/20260425_184000-ping-workload/trace.txt

# 输出示例:
# busybox-102  [000] ..s.. 196.090753: netif_receive_skb: dev=eth0 skbaddr=ffff99ac411bf500 len=84

# 计算 RX 增量
cat rx_end.txt && cat rx_start.txt
echo "delta: $((${rx_end} - ${rx_start}))"
```

### 5. 关键 trace points

| Trace Point | 含义 |
|-------------|------|
| `netif_receive_skb` | skb 进入协议栈前 (RX) |
| `netif_rx` | skb 到达网络层 |
| `net_dev_queue` | skb 进入发送队列 (TX) |
| `napi_poll` | NAPI 轮询处理 |

## 目录结构

```
lab-virtio-net-runtime-observe/
├── START_HERE.md       ← 一页速览
├── README.md           ← 本文件
├── docs/
│   ├── 01_GOAL_AND_HYPOTHESIS.md   ← 学习目标
│   └── 02_TEST_PROCEDURE.md        ← 操作步骤
├── scripts/             ← 实验脚本
└── records/            ← 测试结果
    ├── README.md       ← 测试记录与验证
    └── 20260425_183500-idle-baseline/
    └── 20260425_184000-ping-workload/
```

## 测试结果 (2026-04-25)

| 实验 | RX 增量 | 说明 |
|------|---------|------|
| Idle baseline | +6 | 3秒采样 |
| Ping 20次 | +21 | RTT 0.59ms avg |

详见 `records/README.md`