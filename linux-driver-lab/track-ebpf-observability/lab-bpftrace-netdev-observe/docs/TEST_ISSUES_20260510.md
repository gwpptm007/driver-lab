# 测试问题记录 (2026-05-10)

## 测试机信息
- IP: 192.168.65.135
- 用户: wq7 / wq123456!
- 网卡: ens33 (192.168.65.135), ens192 (192.168.100.1)

---

## 1. lab-af-xdp-socket-rings 测试

### 问题：rx_packets 始终为 0

**现象：**
- XDP attach 成功
- UMEM/socket/rings 创建成功
- XSKMAP 注册成功
- 程序poll循环正常
- 但 `rx_packets=0`，ens192 RX计数器始终为 0

**根本原因：**
单台测试机本地 ping 192.168.100.1 走 loopback，流量不到 ens192 网卡：
```
$ ip route get 192.168.100.1
local 192.168.100.1 dev lo src 192.168.100.1 uid 0
```

Windows 发送 UDP 到 192.168.100.1 经过企业网络网关 (100.75.16.1)，不经过 VM：
```
Windows -> 100.75.16.1 -> 企业网络 -> 192.168.100.1 (不经过VM)
```

**验证：**
- `ip -s link show ens192` RX 计数器一直为 0
- `tcpdump -i ens192` 看不到任何外部流量
- `tcpdump -i ens33` 也没有 192.168.100.1 的流量

**结论：**
AF_XDP 基础设施（代码/编译/XDP attach/socket创建/rings）全部正常，`rx_packets=0` 是网络路由限制，非代码问题。

**解决方案：**
1. 需要外部测试仪/另一台VM直接向ens192发包
2. 或调整网络拓扑让外部流量经过ens192

---

## 2. lab-bpftrace-netdev-observe 测试

### 问题1：bpftrace 安装
- 状态：测试机原本没有 bpftrace
- 解决：`sudo apt-get install bpftrace`

### 问题2：原有 .bt 脚本 BEGIN block 错误

**现象：**
```
ERROR: Could not resolve symbol: /proc/self/exe:BEGIN_trigger
```

**原因：**
bpftrace v0.14.0 的 BEGIN trigger 解析问题

**解决方案：**
使用命令行直接运行 bpftrace，不用脚本

### 问题3：kprobe:netif_receive_skb 无法使用

**现象：**
```
ERROR: BTF: failed to read data (Invalid argument) from: /sys/kernel/btf/vmlinux
ERROR: BTF: failed to find BTF data
```

**原因：**
内核没有 BTF debug info，vmlinux BTF 不可用

**解决方案：**
使用 tracepoint 替代 kprobe：
- `tracepoint:net:netif_receive_skb` 替代 `kprobe:netif_receive_skb`
- `tracepoint:net:net_dev_queue` 替代 `kprobe:dev_queue_xmit`

### 问题4：dev_queue_xmit 是 notrace 内联函数

**现象：**
```
WARNING: dev_queue_xmit is not traceable (either non-existing, inlined, or marked as "notrace")
```

**解决方案：**
使用 `tracepoint:net:net_dev_queue` 或 `tracepoint:net:net_dev_xmit`

---

## 3. 测试验证结果

### track-af-xdp (lab-af-xdp-socket-rings)

| 检查项 | 状态 |
|--------|------|
| 代码同步到测试机 | ✓ |
| 编译成功 | ✓ |
| XDP attach 到 ens192 | ✓ |
| UMEM 创建 (4096 frames) | ✓ |
| AF_XDP socket 创建 (fd=3) | ✓ |
| FILL/RX/TX/COMPLETION rings | ✓ |
| XSKMAP 注册 | ✓ |
| 程序15秒poll循环 | ✓ |
| RX 流量验证 | ⚠️ 网络限制 |

### track-ebpf-observability (lab-bpftrace-netdev-observe)

| 检查项 | 状态 |
|--------|------|
| 代码同步到测试机 | ✓ |
| bpftrace 安装 | ✓ |
| tracepoint:net:netif_receive_skb | ✓ (62 events) |
| tracepoint:net:net_dev_queue | ✓ (54 events) |
| kprobe:netif_receive_skb | ⚠️ BTF问题 |
| kprobe:dev_queue_xmit | ⚠️ notrace |

---

## 4. 后续建议

### AF_XDP
- 找外部测试仪验证真实RX流量
- 或使用另一台同网段VM作为发送方

### bpftrace
- 修改 .bt 脚本，移除 BEGIN block 或使用兼容写法
- 使用 tracepoint 替代 kprobe 作为主要观测点