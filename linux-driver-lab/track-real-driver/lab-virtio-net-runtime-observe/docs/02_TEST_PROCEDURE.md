# 测试流程

## 验证假设

### 假设 1：RX 事件链可观测
```
设备 → callback/wakeup → napi_schedule → poll → netif_receive_skb
```
用 `netif_receive_skb` trace 捕获每个 RX 包。

### 假设 2：TX 完成链可观测
```
start_xmit → net_dev_queue → net_dev_xmit
```
用 `net_dev_queue`, `net_dev_xmit` trace。

### 假设 3：workload 差异可区分
| 状态 | RX delta (3秒) | 特征 |
|------|----------------|------|
| idle | ~5-10 | 最小流量 |
| ping 20次 | ~20-30 | 响应式流量 |
| iperf | 大量 | 持续带宽 |

## 操作步骤

### Step 1: 环境检查

```bash
# Guest 内
mount -t tracefs nodev /sys/kernel/tracing
ls /sys/kernel/tracing/events/net/  # 确认 trace points 存在
busybox ifconfig eth0 up
busybox ifconfig eth0 10.0.2.15 netmask 255.255.255.0
```

### Step 2: Idle baseline

```bash
mkdir -p /lab/records/<ts>-idle-baseline

# 记录初始
cat /sys/class/net/eth0/statistics/rx_packets > /lab/records/<ts>-idle-baseline/rx_start.txt

# 启用 trace
echo 0 > /sys/kernel/tracing/tracing_on
> /sys/kernel/tracing/trace
echo 1 > /sys/kernel/tracing/events/net/netif_receive_skb/enable
echo 1 > /sys/kernel/tracing/tracing_on

# 等待 3-5 秒
sleep 3

# 停止
echo 0 > /sys/kernel/tracing/tracing_on

# 记录结果
cat /sys/class/net/eth0/statistics/rx_packets > /lab/records/<ts>-idle-baseline/rx_end.txt
cat /sys/kernel/tracing/trace > /lab/records/<ts>-idle-baseline/trace.txt
```

### Step 3: Ping workload

```bash
mkdir -p /lab/records/<ts>-ping

# 记录初始
cat /sys/class/net/eth0/statistics/rx_packets > /lab/records/<ts>-ping/rx_start.txt

# 启用 trace
echo 0 > /sys/kernel/tracing/tracing_on
> /sys/kernel/tracing/trace
echo 1 > /sys/kernel/tracing/events/net/netif_receive_skb/enable
echo 1 > /sys/kernel/tracing/tracing_on

# 执行 ping
busybox ping -c 20 10.0.2.2

# 停止
echo 0 > /sys/kernel/tracing/tracing_on

# 记录
cat /sys/class/net/eth0/statistics/rx_packets > /lab/records/<ts>-ping/rx_end.txt
cat /sys/kernel/tracing/trace > /lab/records/<ts>-ping/trace.txt
```

### Step 4: 验证

```bash
# 检查 trace 事件数
grep netif_receive_skb /lab/records/<ts>-ping/trace.txt | wc -l

# 检查 RX 增量
echo "RX delta: $(($(cat rx_end.txt) - $(cat rx_start.txt)))"

# 查看具体 trace
head -20 /lab/records/<ts>-ping/trace.txt
```

## 预期结果

### Idle baseline
```
entries: 5-10
events: netif_receive_skb (偶尔)
```

### Ping workload
```
entries: 20-30
events: netif_receive_skb (每个 ping reply)
ping: 20 transmitted, 20 received, 0% loss
RTT: ~0.5ms avg
```

### 验证通过标准

1. trace.txt 非空，包含 `netif_receive_skb`
2. RX delta > 0
3. Ping 0% loss