# Lab Overview

Phase 5 把前面几阶段的基础能力组合成一个小型 L3 数据面：

```text
pcap PMD -> mbuf -> packet parse -> ACL -> route lookup -> TX burst -> stats
```

它的重点不是性能，而是工程结构：

- 数据面只做固定规则，避免控制面复杂化。
- pcap 生成固定比例的 forward、ACL drop、route miss 流量。
- 输出 `RESULT`、`ROUTE_STATS`、`ACL_STATS`，便于脚本自动验收。

