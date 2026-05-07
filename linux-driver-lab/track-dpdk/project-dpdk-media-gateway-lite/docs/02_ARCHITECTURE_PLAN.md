# 02_ARCHITECTURE_PLAN

## 计划结构

```text
app/
├── main.c
├── port.c / port.h
├── packet.c / packet.h
├── rule.c / rule.h
├── rewrite.c / rewrite.h
├── stats.c / stats.h
└── config.c / config.h
```

## 数据路径

```text
rx_burst
  -> parse ethernet
  -> arp / ipv4 / udp classify
  -> rule lookup
  -> rewrite
  -> tx_burst
  -> stats update
```

## 规则模型

```text
方向: ingress_port -> egress_port
匹配: src/dst ip, src/dst udp port
动作: rewrite mac/ip/port, drop, forward
统计: hit, bytes, drop reason
```
