# 08_INTERVIEW_EXPLANATION

## 面试时怎么讲

可以这样描述：

> 我在 DPDK track 最后做了一个 media-gateway-lite 项目，把前面 testpmd、vhost-user、virtio-user、l2fwd 和 fastpath 的实验收束成一个简化媒体面。程序基于 DPDK poll mode，完成端口初始化、mbuf/mempool、RX/TX burst、Ethernet/ARP/IPv4/UDP 解析、UDP-only 过滤、静态规则匹配、MAC/IP/UDP port 改写和 per-port/per-rule/drop reason 统计。这个项目对应我以前做运营商网元媒体面时的核心路径：只处理 UDP 媒体流，按方向改写二三四层头并转发，控制面通过配置下发规则。

## 能力关键词

```text
DPDK ethdev
PMD
hugepage
mempool/mbuf
rx_burst/tx_burst
UDP-only fast path
L2/L3/L4 rewrite
per-rule stats
drop reason
media-plane gateway
```
