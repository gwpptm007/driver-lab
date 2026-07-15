# 03：Linux bridge、FDB、VLAN 与二层转发

## Linux bridge 的职责

Linux bridge 把多个 host net_device 组成一个二层广播域。它根据 Ethernet 源/目的 MAC 学习和转发帧：

1. 帧从某个 bridge port 进入；
2. bridge 学习“源 MAC -> 入端口”的 FDB 项；
3. 若目的 MAC 已知且允许转发，只从对应端口发出；
4. 若目的未知、广播或需要泛洪，则复制到符合条件的端口；
5. 若目的地址属于 bridge 自己或 local delivery 条件成立，帧还会交给 host 网络栈。

它是 host kernel 软件实现；FDB 命中减少的是无谓的泛洪，不代表“绕过 CPU”。

## FDB 学习是源 MAC 驱动的

假设 guest A 的 MAC 是 `52:54:00:12:34:a1`，通过 `tap-vnet-a` 发送第一帧。bridge 先学习：

```text
52:54:00:12:34:a1  -> tap-vnet-a
```

它不会因为“配置文件里写了 guest A 的 MAC”而天然知道该映射。guest B 回包后会学习 B 的 MAC。之后 A 发给 B 的 unicast 才能被定向转发。

```bash
bridge fdb show br br-vnet0
bridge link show master br-vnet0
```

FDB 是动态事实：端口 down、MAC 漂移、老化、VLAN 变化或显式静态项都会改变结果。记录时要带时间与实验拓扑。

## unknown-unicast、broadcast、multicast

| 帧类型 | 未学习时的动作 | 为什么实验会看到它 |
| --- | --- | --- |
| broadcast | flood | ARP request、某些控制协议 |
| unknown unicast | flood（受配置/端口状态约束） | 对端 MAC 尚未被学习 |
| known unicast | 只转发到 FDB 指向端口 | 双向 ARP/ICMP 后常见 |
| multicast | 由 bridge/multicast snooping 策略决定 | IPv6 ND、组播应用 |

不要把一次 TAP 抓到的多份 ARP 包误认为桥发生环路；先判断它是否是正常 flood，以及哪些端口在观察。

## guest-to-host 与 guest-to-guest

### guest 到 host bridge IP

guest 先 ARP host bridge IP，后续 IP 包的目的 MAC 是 bridge 设备的 MAC。bridge 一方面会学习 guest 源 MAC，另一方面把目的为本地 MAC 的帧交给 host 本地协议栈。此时 host 回包还要经 bridge 选择 TAP egress。

### guest A 到 guest B

两个 TAP 都接入同一 bridge 后，目的 MAC 为 guest B 的帧在 FDB 命中时由 bridge 从 `tap-vnet-b` 发出。它不需要 host IP forwarding 或 host 路由表参与；如果 host 抓包工具、br_netfilter 或安全策略介入，仍可能看到额外处理，但不能因此混淆基础 L2 路径。

## VLAN、STP 与 hairpin：先知道边界再启用

- **VLAN filtering**：bridge 可以按 VLAN 处理端口和 FDB。启用后，端口 PVID/allowed VLAN 与帧标签共同决定可达性。基础 Lab 不应在没有证据前假设 VLAN 已工作。
- **STP**：有环拓扑时防止二层环路；学习/forwarding 可能因端口状态延迟或阻塞而不同。单 bridge、两个 TAP 的最小实验通常先保持拓扑无环。
- **hairpin mode**：决定帧能否从同一 port 回送。它是特定虚拟化/overlay 需求，不是普通 guest-to-guest bridge forwarding 的前置条件。
- **br_netfilter**：可能让 bridge 流量被 IP netfilter 观察/处理。排障时必须记录它是否启用，不能把 L2 行为和 NAT/iptables 行为混为一谈。

## 证据优先级

1. `bridge link`：端口归属与状态；
2. `bridge fdb`：学习后的转发表；
3. TAP/bridge 抓包：某时刻某接口可见的帧；
4. guest `ip neigh`、`ping`：端到端协议结果；
5. bridge/VLAN/STP 配置：解释为什么某端口能或不能转发。

这五项共同能支持“guest A 经 bridge 到 guest B”的结论；任意单项都不够。

## 扩展方向

本章模型可平移到物理 NIC 加入 bridge、macvtap、OVS、VXLAN/EVPN 与 switchdev offload。扩展时先明确 FDB 在哪里维护、学习由谁做、帧是否仍会经过 host CPU，再讨论性能。对应实践：[双 guest L2 转发](../../lab-two-guest-bridge-flow/docs/03_L2_FORWARDING_MODEL.md)。
