# 01_GOAL_AND_SCOPE

## 目标

实现一个简化版 DPDK 用户态媒体网关，用于把前面 `fastpath-lite` 的能力整理成项目型作品。

## 范围内

- DPDK EAL / mempool / ethdev 初始化；
- 单 RX/TX queue 的 poll-mode 数据路径；
- Ethernet / ARP / IPv4 / UDP 解析；
- UDP-only 策略；
- 静态规则表：方向、匹配、rewrite；
- MAC / IPv4 / UDP port rewrite；
- per-port / per-rule / drop reason 统计；
- 可复现脚本、records、review bundle。

## 范围外

- 完整 SIP/RTP/RTCP 协议栈；
- 完整 NAT ALG；
- **KNI 或 tap 回注**（但见下方架构说明）；
- rte_flow 硬件 offload；
- 多进程控制面。

## 架构说明：DPDK 与 KNI 的协同

运营商媒体网关的标准架构是 **DPDK 处理业务流量（ARP重组、MAC层、IP层、UDP层、转发），其他协议走 KNI 回注内核**：

```
物理网卡 (ens192)
       │
       ├── 业务流量
       │    ├── ARP 重组 ──→ DPDK 业务面
       │    ├── MAC 层解析 ──→ DPDK 业务面
       │    ├── IP 层解析 ──→ DPDK 业务面
       │    ├── UDP 层解析 ──→ DPDK 业务面
       │    └── 转发到下一级虚机网口 ──→ DPDK 业务面
       │
       └── 非业务流量 ──→ KNI ──→ Linux 内核（DNS/SIP 呼叫/SSH 管理等）

```

| 流量类型 | 处理路径 |
|----------|----------|
| ARP 重组 | DPDK 业务面 |
| MAC 层 | DPDK 业务面 |
| IP 层 | DPDK 业务面 |
| UDP 层 | DPDK 业务面 |
| 转发到下一级 VM | DPDK 业务面 |
| DNS/TCP/SIP 呼叫/SSH | KNI → Linux 内核 |

**当前项目** `media-gateway-lite` 实现了纯 DPDK 接收 + UDP-only 过滤 + 简单 rewrite，非 UDP 流量直接丢弃。
后续如果要做得更接近生产环境，可以加上 KNI 支持，把非业务流量打回内核。
