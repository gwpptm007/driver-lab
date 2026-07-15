# 11：虚拟网络分层排障手册

## 总原则：从静态关系到动态帧，再到队列/性能

不要一开始同时重建 TAP、bridge、QEMU、guest IP 和 vhost。每次只验证一层；下一层以前一层的明确证据为前提。

## 0. 安全与现场保护

- 确认操作的是实验 bridge/TAP，不是管理口或生产网卡；
- 保存当前 `ip -br link`、`ip addr`、`bridge link/fdb` 和 QEMU command line；
- 停止不确定的自动脚本；不要用广泛的 `ip link delete` 或全局 flush 破坏其他实验。

## 1. QEMU 进程或 guest 网卡不存在

| 症状 | 先检查 | 常见原因 |
| --- | --- | --- |
| QEMU 启动失败 | QEMU stderr、TAP owner、`ifname` | TAP 不存在、权限不足、参数拼写错误 |
| guest 没有接口 | `ip -br link`、`dmesg`、`lspci` | `-device` 缺失、设备模型/驱动不匹配 |
| guest MAC 不符 | QEMU `mac=`、guest `ip link` | 多个参数/默认 MAC、观察了错误接口 |

先确保 `-netdev id=...` 与 `-device netdev=...` 一一对应。

## 2. TAP 或 bridge 拓扑错误

```bash
ip -d link show tap-vnet0
ip -d link show br-vnet0
bridge link
bridge fdb show br br-vnet0
```

检查 TAP 是否 UP、是否为 bridge port、bridge 是否 UP、端口是否处于允许转发的状态。TAP 存在但未加入 bridge 时，guest 不会自动到达 host bridge IP 或另一 TAP。

## 3. guest 能发包但 host 看不到

按顺序检查：

1. guest 接口 UP、IP/route 指向预期接口；
2. guest 是否发 ARP/ICMP（guest 抓包/neighbor）；
3. QEMU 是否仍持有正确 TAP，是否有 stderr；
4. host 在 TAP 观察点抓包；
5. 再看 vhost 模式和 virtqueue/guest driver 问题。

若 host TAP 完全无帧，先不要怀疑 bridge FDB；帧还没到 bridge。

## 4. host 看见 request，但 guest 收不到 reply

检查 host `br-vnet0` 是否有正确 IP、host 防火墙/策略、host 是否产生 reply、bridge FDB 是否有 guest MAC、reply 是否出现在 TAP。若 reply 已离开 TAP 而 guest 没收到，才深入 guest RX/virtqueue/backend。

## 5. 两个 guest 互不通

把问题拆成 A 的 ARP request 是否到 B、B 的 ARP reply 是否回 A：

| 断点 | 优先检查 |
| --- | --- |
| A 无 ARP request | A IP/route/interface state |
| request 到 A TAP 不到 B TAP | bridge port membership、VLAN/STP、安全策略 |
| B 收 request 不回 reply | B IP/interface/neigh/防火墙 |
| reply 到 B TAP 不到 A TAP | FDB、bridge port state、VLAN |
| ARP 完成仍无 ICMP | guest firewall、IP 配置、MTU、过滤规则 |

## 6. vhost=on 失败或无差异

先回到 `vhost=off` 证明基础 TAP/bridge 路径，再检查：

```bash
ls -l /dev/vhost-net
lsmod | grep -E 'vhost_net|vhost'
```

记录 QEMU 错误。`vhost=on` 和 off 都能 ping 不表示“没有差异”，只表示基本可达；路径差异需要状态、性能或 tracing 证据，并控制变量。

## 7. 性能/计数看起来矛盾

先记录 offload、包大小、采集点、时间窗口和统计粒度。GRO/GSO、batch、queue 统计和抓包点会导致“包数不相等”；先判断是否粒度不同，再判断是否丢包。禁止在未记录工作负载/CPU 的情况下比较两轮吞吐。

## 最小恢复策略

失败后优先执行可逆动作：停止 QEMU、保存证据、删除自己创建的 guest IP/TAP/bridge，重新按 Lab 1 最小闭环建立。不要带着不明残留直接进入双 guest 或 vhost 对照。
