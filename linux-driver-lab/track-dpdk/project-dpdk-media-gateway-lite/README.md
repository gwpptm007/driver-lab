# project-dpdk-media-gateway-lite

> 计划中的下一阶段项目：简化版 DPDK 用户态媒体网关。

## 当前状态

```text
PLANNED / DO_NOT_START_BEFORE_TRAFFIC_TEST
```

不要现在直接实现。本项目依赖：

```text
project-fastpath-traffic-test >= PASS_TRAFFIC
最好达到 PASS_FORWARDING
```

## 项目定位

把 `fastpath-lite` 从教学/验证程序升级为项目型作品：

```text
UDP-only media fast path
规则驱动 rewrite
双方向转发
按规则统计
drop reason
records + interview notes
```

## 计划能力

| 模块 | 目标 |
|---|---|
| `port` | 端口初始化、queue、promisc、stats |
| `packet` | Ethernet/ARP/IPv4/UDP 解析 |
| `rule` | 方向、匹配、rewrite 配置 |
| `rewrite` | MAC/IP/UDP port 改写 |
| `stats` | per-port/per-rule/drop reason 统计 |
| `control` | env/config 文件驱动 |

## 推荐进入条件

- `project-fastpath-traffic-test` 已有真实 UDP 流量记录；
- records 中 `rx/ipv4/udp` 非 0；
- rewrite demo 可触发，或至少确认规则加载路径无误；
- 明确测试拓扑是单口 RX、双口转发，还是 vhost/virtio-user。
