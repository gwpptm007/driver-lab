# DPDK_INTERVIEW_ANSWER_CARD

## 30 秒版本

```text
我做过 DPDK 用户态数据面项目，旧项目基于 DPDK v17，主要处理虚拟化/网元场景下的 UDP 媒体流。核心路径是 PMD 批量收包，用户态解析 Ethernet/IPv4/UDP，根据规则做 MAC/IP/UDP 改写并 tx_burst 发出。最近我又用现代 DPDK 环境复现了一条完整学习链，包括 vmxnet3/testpmd、vhost-user、virtio-user、自写 l2fwd-lite、fastpath-lite 和 media-gateway-lite，用 records 区分 smoke、traffic、forwarding、rewrite 验收等级。
```

## 2 分钟版本

```text
DPDK 这块我不是只跑过 testpmd。旧项目里我接触的是 DPDK v17 的媒体面转发，场景是虚拟化网元里的 UDP 流量处理。数据路径上，网卡队列由 DPDK PMD 接管，通过 hugepage、mempool、mbuf 管理包内存，用 rx_burst 批量收包。应用层只处理 fastpath 需要的 Ethernet、IPv4、UDP 头，根据网元方向做 MAC、IP、UDP 端口改写，然后 tx_burst 发出。控制类或异常类流量以前会考虑 KNI 或内核辅助路径。

为了把旧经验和现代 DPDK 对齐，我现在做了一个 track，从 vmxnet3 PMD smoke、vhost-user backend、virtio-user frontend，到自写 l2fwd-lite、fastpath-lite 和 media-gateway-lite。media-gateway-lite 里我把端口、配置、包解析、规则、统计拆成模块，当前完成了双端口 vdev smoke 和 UDP-only drop 路径验证，后续补真实 UDP traffic、forwarding 和 rewrite records。
```

## 被追问 KNI

```text
KNI 在旧版本里常用于把 DPDK 用户态路径和 Linux 内核协议栈连接起来，适合处理少量控制流量或需要内核网络功能的报文。但现在重新设计时我不会默认优先选 KNI，会根据部署环境评估 tap、vhost-user、virtio-user、AF_XDP 或独立控制面通道。
```

## 被追问不足

```text
我会明确区分验收等级。media-gateway-lite 已完成 pcap + null PMD 下的 `PASS_PCAP_FUNCTIONAL/FORWARDING/REWRITE`，证明软件 parser、rule、rewrite 和 ownership 路径；它不等于外部 wire、真实 NIC 或性能验证，后续分别补 `PASS_EXTERNAL_TRAFFIC`、`PASS_REAL_NIC_FORWARDING` 和 `PASS_PERFORMANCE`。
```
