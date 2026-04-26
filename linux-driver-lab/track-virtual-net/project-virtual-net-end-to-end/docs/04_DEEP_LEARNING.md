# 05_DEEP_LEARNING

## 实测结果汇总

### Test 1: Single Guest Ping Host Bridge (vhost=off)

```
PING 192.168.100.1 (192.168.100.1): 56 data bytes
64 bytes from 192.168.100.1: seq=0 ttl=64 time=8.770 ms
64 bytes from 192.168.100.1: seq=1 ttl=64 time=0.616 ms
64 bytes from 192.168.100.1: seq=2 ttl=64 time=1.232 ms
round-trip min/avg/max = 0.616/3.539/8.770 ms
```

**路径**: guest eth0 → QEMU virtio-net-pci → tap-vnet0 → br-vnet0 → host IP stack

**特点**: 目的 MAC = bridge 自身 MAC，包进入 host IP 协议栈处理

---

### Test 2: Single Guest Ping Host Bridge (vhost=on)

```
PING 192.168.100.1 (192.168.100.1): 56 data bytes
64 bytes from 192.168.100.1: seq=0 ttl=64 time=8.162 ms
64 bytes from 192.168.100.1: seq=1 ttl=64 time=0.786 ms
64 bytes from 192.168.100.1: seq=2 ttl=64 time=0.565 ms
round-trip min/avg/max = 0.565/3.171/8.162 ms
```

**路径**: guest eth0 → QEMU virtio-net-pci → vhost_net kernel backend → tap-vnet0 → br-vnet0 → host IP stack

**与 vhost=off 对比**: RTT 差异不大 (3.171ms vs 3.539ms avg)，因为 ping 包量小且 bridge forwarding 路径已足够快

---

### Test 3: Two Guest L2 Flow (guest A ping guest B via bridge)

guest A init 脚本:
```
[init] guest A start
[init] pinging host...
ping -c 2 192.168.100.1  → 成功
[init] pinging guest B...
ping -c 3 192.168.100.3  → 成功
```

**ping 结果**:
```
--- 192.168.100.1 ping statistics ---
2 packets transmitted, 2 packets received, 0% packet loss
round-trip min/avg/max = 0.742/4.063/7.384 ms

--- 192.168.100.3 ping statistics ---
3 packets transmitted, 3 packets received, 0% packet loss
round-trip min/avg/max = 1.370/2.872/5.810 ms
```

**FDB 学习结果**:
```
52:54:00:12:34:a1 dev tap-vnet-a master br-vnet0
52:54:00:12:34:b1 dev tap-vnet-b master br-vnet0
```

**bridge link 状态**:
```
tap-vnet-a: state forwarding
tap-vnet-b: state forwarding
```

**路径**: guest A → tap-vnet-a → br-vnet0 (FDB 查 b1 → tap-vnet-b) → guest B

**关键点**: guest-to-guest 流量目的 MAC ≠ bridge 自身 MAC，bridge 查 FDB 直接从对应 tap 端口转发，不上 host IP 栈

---

## 三个测试场景串联

| 测试 | 场景 | 路径 | 关键理解 |
|------|------|------|----------|
| Test 1 | single guest vhost=off | guest → tap → bridge → host IP | QEMU userspace backend |
| Test 2 | single guest vhost=on | guest → tap → bridge → host IP | vhost_net kernel backend |
| Test 3 | two guest L2 flow | guest A → tapA → bridge → tapB → guest B | FDB 命中，纯 L2 转发 |

---

## 与前面 Lab 的关系

| Lab | 路径 | 本项目对应测试 |
|-----|------|---------------|
| lab-virtio-tap-bridge-path | guest → tap → bridge → host IP | Test 1/2 |
| lab-virtio-vhost-kick-notify | vhost=off/on 对比 | Test 1/2 |
| lab-two-guest-bridge-flow | guest A → tapA → bridge → tapB → guest B | Test 3 |

---

## 收尾结论

1. **vhost=off vs vhost=on**: 在小流量 ping 测试中差异不明显；大量数据时 kernel backend 优势才显现
2. **FDB 学习**: 两个 guest 均发包后，bridge 同时学习两个 MAC；FDB 命中后直接转发
3. **纯 L2 转发**: guest-to-guest 流量不经过 host IP stack，是 bridge L2 forwarding
4. **收尾**: track-virtual-net 三个 lab + 本项目完成了 virtio_net + tap + bridge 的完整路径验证