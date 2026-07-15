# 07：单 guest 的 TAP/bridge 数据路径

## 实验目标

单 guest Lab 的目标不是“创建一个能 ping 的 QEMU”，而是把 guest 到 host bridge IP 的路径拆成可观察、可复现的层。建议固定：

```text
host br-vnet0:      192.168.100.1/24
host tap-vnet0:     bridge port
guest virtio eth0:  192.168.100.2/24
```

## ARP 先于 ICMP

首次 ping 通常包含两个阶段：

1. guest 发 ARP request：广播帧询问 `192.168.100.1` 的 MAC；
2. host bridge/local stack 处理并回 ARP reply，guest 建立邻居项；
3. guest 发 ICMP echo request；
4. host 回 echo reply，bridge 根据 FDB/port 关系把回包送回 TAP。

所以第一次抓包中出现 ARP、广播、可能的未学习泛洪是正常现象。要区分“ARP 尚未完成”与“ICMP 已发送但丢失”。

## TX 方向的分层检查

```text
guest route selects virtio-net
  -> guest TX queue / virtqueue
  -> QEMU or vhost backend
  -> host TAP ingress
  -> bridge learns guest source MAC
  -> local delivery to br-vnet0 IP stack
```

每一层的验证问题：

| 层 | 检查 | 成功意味着 |
| --- | --- | --- |
| guest L3 | `ip route get 192.168.100.1` | 会从预期接口发送 |
| guest L2 | `ip neigh`、guest MAC | 邻居与源 MAC 可解释 |
| host TAP | `tcpdump -ni tap-vnet0` | 帧到达/离开 TAP 观察点 |
| bridge | `bridge link/fdb` | port 归属和 MAC 学习正确 |
| host L3 | `ip addr show br-vnet0`、ICMP reply | host 接受并回答该地址 |

## 回程路径不要省略

host reply 的源 MAC 是 bridge 的 MAC，目的 MAC 是 guest MAC。bridge 查 FDB 或按适用规则转发到 `tap-vnet0`；TAP backend 将帧提交 guest RX virtqueue；guest driver/NAPI 最终把 reply 交给 ping 进程。

如果只在 guest 一侧看见 request、host 不回包，问题可能在 bridge IP/local delivery；如果 host 已抓到 reply、guest 没收到，问题更可能在 TAP backend、virtqueue RX 或 guest 接口状态。方向不同，排查顺序不同。

## 必须记录的最小证据集

```bash
# host
ip -br link
ip addr show dev br-vnet0
bridge link
bridge fdb show br br-vnet0
tcpdump -ni tap-vnet0 -e -vv icmp or arp

# guest
ip -br link
ip addr show dev <guest-ifname>
ip route
ip neigh
ping -c 5 192.168.100.1
```

抓包建议同时记录 Ethernet MAC（`-e`）；只记录 IP 地址会丢掉判断 FDB/local delivery 所需的关键事实。

## 不应过度推断

- ICMP 成功不等于 virtio multiqueue、offload 或 vhost 已工作；
- FDB 有 guest MAC 不等于 host 回包一定通过正确的 virtqueue 被 guest 消费；
- TAP 处有流量不等于 host bridge 已配置正确 IP；
- host 与 guest 在同网段不等于没有防火墙、rp_filter、namespace 或 bridge VLAN 问题。

## 清理也是实验的一部分

停机后删除自己创建的 IP、TAP 和 bridge，确认 QEMU 已退出且没有残留 bridge port。共享宿主机上不要盲删非本实验创建的接口。清理记录能防止下一轮误把旧 FDB、旧 IP 或旧 TAP 当作新实验结果。
