# 08：双 guest、FDB 学习与二层转发

## 从单 guest 升级到双 guest，新增的不是一条 ping

双 guest Lab 的核心价值是把 host bridge 从“接收本地 IP 的接口集合”变成真正可验证的二层交换域。拓扑为：

```text
guest A eth0 -- virtio/QEMU -- tap-vnet-a --+
                                            br-vnet0
guest B eth0 -- virtio/QEMU -- tap-vnet-b --+
```

guest A/B 位于同一 IP 子网时，A 发给 B 的 IPv4 ping 先触发 ARP；这会自然展示 broadcast flood、源 MAC 学习、ARP reply、known-unicast forward 这条完整 L2 行为链。

## 逐帧理解第一轮通信

1. A 发 ARP request，源 MAC 为 A，目的 MAC 为广播；bridge 从 `tap-vnet-a` 学习 A MAC，并将广播转到合格端口与 local delivery 路径。
2. B 收到 ARP request 后产生 ARP reply，源 MAC 为 B、目的 MAC 为 A；bridge 从 `tap-vnet-b` 学习 B MAC。
3. bridge 已知 A MAC 所在端口，因此把 B 的 ARP reply 定向转到 `tap-vnet-a`。
4. A/B 都有邻居项后，ICMP echo request/reply 多数为 known-unicast；bridge 按 FDB 定向转发。

```bash
bridge fdb show br br-vnet0
# 期待能在不同 TAP port 上看到 A、B 的 MAC；具体条目会随老化和系统配置变化。
```

## “不经过 host IP 栈”的精确定义

对同一 bridge 内、目的为 B MAC 的 guest A 数据帧，host 不需要根据 host 路由表选择下一跳，也不需要把它交给 host IP forwarding 才能到 B。这就是“不会经过 host IP 栈”的含义。

但以下例外或附加处理可能存在：

- 目的为 bridge 自己 MAC/IP 的帧会 local delivery；
- 广播/组播可能被 host 同时观察或处理；
- bridge netfilter、安全策略、VLAN/STP、tc/BPF、抓包工具可能在路径上观察/影响；
- bridge forwarding 本身仍在 host kernel CPU 上执行。

因此应写“基础 L2 forwarding 不依赖 host L3 routing”，而不是写“host 完全不参与”。

## FDB 不是永久真相

FDB 条目具有端口、MAC、VLAN/flags、老化等语义。实验中可能遇到：

| 现象 | 合理解释 | 应检查 |
| --- | --- | --- |
| 首包泛洪 | 目的 MAC 尚未学习 | ARP/邻居状态、抓包时序 |
| FDB 中没有条目 | 条目老化、抓包/流量不足、端口未 forwarding | `bridge link`、再次产生流量 |
| MAC 出现在错误 port | TAP/QEMU MAC 配置冲突、接线错误、MAC 漂移 | QEMU 参数、guest `ip link` |
| A/B ping 不通但都能 ping host | bridge port/VLAN/FDB 或 B guest 配置问题 | A->B ARP request/reply 两方向 |

## 观测矩阵

| 目标 | guest A | host TAP A | bridge | host TAP B | guest B |
| --- | --- | --- | --- | --- |
| A ARP request | 发出 | 可见 | 学习 A + flood | 可见 | 接收 |
| B ARP reply | 接收 | 可见 | 学习 B + 定向 A | 通常不作为 egress | 发出 |
| A->B ICMP | 发出 | 可见 | FDB 命中 B | 可见 | 接收 |
| B->A ICMP reply | 接收 | 可见 | FDB 命中 A | 可见 | 发出 |

表中“可见”受抓包点、offload、抓包过滤和时间窗口影响；它是预期观测设计，不是所有环境绝对逐包保证。

## 从二层 Lab 扩展的正确顺序

1. 固定两个 guest MAC/IP 和两个 TAP；
2. 记录 port membership/FDB；
3. 只验证 ARP 和 ICMP；
4. 再增加 VLAN filtering、multicast、限速、tc/BPF 或物理 uplink；
5. 每引入一个特性，更新拓扑、预期 FDB 和验收标准。

这样扩展时不会把“基础 L2 不通”和“新策略阻断”混为同一个问题。对应实践：[双 guest Lab](../../lab-two-guest-bridge-flow/START_HERE.md)。
