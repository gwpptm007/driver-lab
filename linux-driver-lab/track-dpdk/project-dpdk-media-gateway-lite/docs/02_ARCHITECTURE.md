# 02_ARCHITECTURE

## 模块结构

```text
app/
├── main.c              # EAL、主循环、RX/TX burst
├── gateway_config.*    # 参数解析、默认配置
├── gateway_port.*      # ethdev 端口初始化
├── gateway_packet.*    # Ethernet/ARP/IPv4/UDP 分类
├── gateway_rule.*      # 规则表、匹配、rewrite
└── gateway_stats.*     # 软件统计与 ethdev stats
```

## 数据路径

```text
rte_eth_rx_burst
  -> Ethernet classify
  -> ARP / IPv4 / UDP classify
  -> rule lookup by ingress port + optional tuple
  -> MAC/IP/UDP rewrite
  -> rte_eth_tx_burst
  -> per-port/per-rule/drop stats
```

## 当前模型

为了保持项目可读，当前版本使用：

```text
single lcore main loop
one RX queue per port
one TX queue per port
static rules from CLI/env
```

后续可以扩展到多队列、per-lcore stats、control plane 热更新。
