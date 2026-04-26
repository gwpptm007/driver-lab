# 05_DPDK_V17_TO_MODERN_MAPPING

## 旧 DPDK v17 经验映射

| 旧项目能力 | 当前 track 对应 |
|---|---|
| UDP 收发 | `lab-dpdk-l2-forwarding` / `project-user-space-fastpath` |
| ARP/IP/UDP 重写 | project 中的 header rewrite 扩展 |
| KNI 回内核 | 后续可和 tap/AF_XDP 对照 |
| 按网元转发 | flow table / route table 扩展 |
| 媒体面收包 | PMD RX burst |
| 多网元转发 | multi-port / multi-queue 扩展 |
