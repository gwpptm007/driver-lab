# 02_L2_FORWARDING_MODEL

## guest-to-host vs guest-to-guest

### guest-to-host（前面的 lab-virtio-tap-bridge-path）

```
guest -> tap -> bridge -> host local IP stack
```

目的 MAC 是 host bridge 设备 MAC，包进入 host 协议栈处理（ARP/ND/IP layer）。

### guest-to-guest（本轮 lab-two-guest-bridge-flow）

```
guest A -> tapA -> bridge -> tapB -> guest B
```

bridge 根据 FDB 做二层转发：
- 目的 MAC 已学习 → 直接从对应 tap 端口转发（不上 IP 栈）
- 目的 MAC 未学习 → flooding 到所有 bridge port

**关键点**：guest-to-guest 流量不经过 host IP stack，是纯 L2 forwarding。

## 实测 FDB 学习结果

```
52:54:00:12:34:a1 dev tap-vnet-a master br-vnet0
52:54:00:12:34:b1 dev tap-vnet-b master br-vnet0
```

- guest A 发包 → bridge 学习源 MAC 在 tap-vnet-a
- guest B 发包 → bridge 学习源 MAC 在 tap-vnet-b
- 双向通信建立后，FDB 命中直接转发，无需 flooding

## 需要避免的误解

### 误解 1：Linux bridge 不经过 CPU
不对。Linux bridge 是 host kernel software bridge，转发逻辑在 host CPU 上执行。

### 误解 2：guest-to-guest 一定经过 host IP stack
不一定。同一 L2 bridge 内，FDB 命中后走 bridge L2 forwarding，不上 IP 协议栈。

### 误解 3：ping 通就说明路径完全理解了
不够。还需要 FDB 学习记录、bridge link 状态等证据。