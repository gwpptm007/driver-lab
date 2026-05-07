# 05_MEDIA_GATEWAY_MIGRATION

## 从旧媒体面到当前 media-gateway-lite

旧 DPDK v17 媒体面项目可以抽象为：

```text
UDP media packet in
  -> classify
  -> direction/rule match
  -> header rewrite
  -> forward out
```

当前 `project-dpdk-media-gateway-lite` 对应拆分为：

```text
main.c
  -> gateway_config
  -> gateway_port
  -> gateway_packet
  -> gateway_rule
  -> gateway_stats
```

## 模块映射

| 旧项目能力 | media-gateway-lite 模块 | 当前状态 |
|---|---|---|
| EAL / hugepage / PMD 初始化 | `gateway_port.c` | 已有 smoke 验证 |
| 包解析 | `gateway_packet.c` | 框架已实现，真实 UDP 待补测 |
| UDP-only | `gateway_rule.c` / `main.c` | net_null 下 drop_non_udp 已验证 |
| MAC/IP/UDP rewrite | `gateway_rule.c` | 框架已实现，rewrite hit 待补测 |
| per-port stats | `gateway_stats.c` | 已有 stats 输出 |
| per-rule stats | `gateway_stats.c` | 框架已实现，真实 hit 待补测 |
| 双端口转发 | `main.c` | vdev smoke 已跑，真实 forwarding 待补测 |

## 当前已验证

来自 `project-dpdk-media-gateway-lite` 记录：

```text
PASS_SMOKE
PASS_UDP_ONLY_DROP_PATH
```

可证明：

```text
EAL 初始化成功
双 vdev port 启动成功
主 loop 正常运行
stats 正常打印
udp_only=1 时 non-UDP drop 路径能被统计
```

## 后面还要补

```text
PASS_TRAFFIC:
  rx_ipv4 > 0
  rx_udp > 0

PASS_FORWARDING:
  tx > 0
  rule_hit > 0

PASS_REWRITE:
  rewrite_hit > 0
  rewrite 后抓包或统计可证明
```

## 建议补测路径

优先级从易到难：

```text
1. pcap PMD:
   UDP pcap -> net_pcap rx -> media-gateway-lite -> net_null tx

2. vhost/virtio-user:
   testpmd txonly -> virtio-user -> vhost-user -> media-gateway-lite

3. 真实 vmxnet3:
   外部 VM/宿主机 -> ens192/vmxnet3 -> media-gateway-lite
```

## 迁移表达

面试时可以这样讲：

```text
我把旧项目里的媒体面逻辑拆成端口、包解析、规则、改写和统计五个模块，在当前 modern DPDK track 中用 media-gateway-lite 重新实现。当前已经完成可启动和双端口 smoke，后续会补真实 UDP 流量、forwarding 和 rewrite 的 records，用来证明它不是单纯 demo，而是具备可验证数据路径的项目型实现。
```
