# 04_DEEP_LEARNING - DPDK L3 Forwarder Lite 深度学习

> Phase 5 是当前 track 里最像“项目”的部分：它把 RX、parse、ACL、route、TX、stats 串成一个小型 L3 数据面。

## 1. 项目要解决的问题

前面阶段分别学了：

```text
mbuf/mempool
RSS boundary
burst/cache tuning
VFIO/IOMMU boundary
```

Phase 5 把这些能力组合成一个数据面骨架：

```text
pcap PMD input -> IPv4/UDP parse -> ACL -> route lookup -> net_null TX
```

## 2. 总体拓扑

```mermaid
flowchart LR
    Gen["tools/gen_l3_pcap.py"] --> Pcap["l3_input.pcap"]
    Pcap --> RXPMD["net_pcap0\nRX port 0"]
    RXPMD --> App["dpdk-l3-forwarder-lite"]
    App --> TXPMD["net_null1\nTX port 1"]
    App --> Log["RESULT / ROUTE_STATS / ACL_STATS"]
```

为什么用 pcap PMD：

```text
可复现，不依赖真实网卡，不影响测试机网络。
```

为什么用 net_null：

```text
能验证 TX burst 成功，同时不需要真实链路对端。
```

## 3. 数据面 pipeline

```mermaid
flowchart TD
    RX["rte_eth_rx_burst(port0)"] --> Eth{"Ethernet type IPv4?"}
    Eth -->|no| NonIPv4["non_ipv4_drops++\nfree mbuf"]
    Eth -->|yes| IP{"IPv4 header valid?"}
    IP -->|no| ParseDrop["parse_drops++\nfree mbuf"]
    IP -->|yes| UDP{"protocol UDP?"}
    UDP -->|no| ParseDrop
    UDP -->|yes| ACL{"UDP dst port == 9999?"}
    ACL -->|yes| ACLDrop["acl_drops++\nACL_STATS[0]++\nfree mbuf"]
    ACL -->|no| Route{"dst in 10.20.0.0/24?"}
    Route -->|no| Miss["route_miss_drops++\nfree mbuf"]
    Route -->|yes| Tx["rte_eth_tx_burst(port1)"]
    Tx --> Sent{"sent == 1?"}
    Sent -->|yes| Fwd["forwarded_packets++\nROUTE_STATS[0]++"]
    Sent -->|no| Fail["tx_failed++\nfree mbuf"]
```

关键顺序：

```text
先 ACL，再 route。
```

原因：安全策略优先于转发策略。

## 4. 程序结构 UML

```mermaid
classDiagram
    class app_config {
        nb_mbuf
        mbuf_cache
        burst_size
        in_port
        out_port
        routes[MAX_ROUTES]
        acl[MAX_ACL_RULES]
    }

    class route_rule {
        prefix_be
        mask_be
        prefix_len
        out_port
        hits
        bytes
    }

    class acl_rule {
        udp_dst_port
        drops
        bytes
    }

    class app_stats {
        rx_packets
        forwarded_packets
        acl_drops
        route_miss_drops
        non_ipv4_drops
        parse_drops
        tx_failed
    }

    app_config "1" --> "*" route_rule
    app_config "1" --> "*" acl_rule
    app_config "1" --> "1" app_stats
```

## 5. 初始化时序

```mermaid
sequenceDiagram
    participant Main as main()
    participant EAL as DPDK EAL
    participant Pool as rte_mempool
    participant RX as net_pcap0
    participant TX as net_null1

    Main->>Main: load_default_rules()
    Main->>EAL: rte_eal_init()
    Main->>Pool: rte_pktmbuf_pool_create()
    Main->>RX: setup_rx_port(0)
    RX-->>Main: RX queue ready
    Main->>TX: setup_tx_port(1)
    TX-->>Main: TX queue ready
    Main->>Main: run_loop()
```

对应配置：

```text
ROUTE[0] prefix=10.20.0.0/24 out_port=1
ACL[0] action=drop udp_dst_port=9999
```

## 6. Packet 分类状态图

```mermaid
stateDiagram-v2
    [*] --> Received
    Received --> NonIPv4Drop: not IPv4
    Received --> ParseDrop: malformed IPv4/UDP
    Received --> ACLCheck: IPv4 UDP
    ACLCheck --> ACLDrop: dst port 9999
    ACLCheck --> RouteLookup: not ACL
    RouteLookup --> RouteMissDrop: no route
    RouteLookup --> Forward: 10.20.0.0/24
    Forward --> TxOk: tx_burst sent
    Forward --> TxFail: tx_burst failed
    TxOk --> [*]
    ACLDrop --> [*]
    RouteMissDrop --> [*]
    NonIPv4Drop --> [*]
    ParseDrop --> [*]
    TxFail --> [*]
```

## 7. 测试流量设计

`tools/gen_l3_pcap.py` 生成三类包：

| 流量 | 数量 | 预期 |
|---|---:|---|
| `10.20.0.77:9000` | 24 | route hit + forward |
| `10.20.0.77:9999` | 12 | ACL drop |
| `10.99.0.77:9000` | 12 | route miss drop |

```mermaid
pie title Phase 5 traffic mix
    "forward" : 24
    "ACL drop" : 12
    "route miss" : 12
```

## 8. 运行时序

```mermaid
sequenceDiagram
    participant Script as 02_run_pcap_l3_forward.sh
    participant Pcap as gen_l3_pcap.py
    participant App as dpdk-l3-forwarder-lite
    participant RX as net_pcap0
    participant TX as net_null1
    participant Rec as records/

    Script->>Pcap: generate 48 packets
    Pcap-->>Rec: l3_input.pcap
    Script->>App: start with --vdev net_pcap0 --vdev net_null1
    loop pcap drain
        App->>RX: rte_eth_rx_burst()
        RX-->>App: mbuf batch
        App->>App: parse + ACL + route
        App->>TX: tx_burst for route hits
    end
    App-->>Rec: RESULT / ROUTE_STATS / ACL_STATS
```

## 9. 当前 evidence

正式记录：

```text
records/20260629-213104-l3-forwarder/
```

关键结果：

```text
rx_packets=48
forwarded_packets=24
acl_drops=12
route_miss_drops=12
tx_failed=0
ROUTE_STATS[0] hits=24
ACL_STATS[0] drops=12
```

这证明：

```text
pcap RX -> parse -> ACL -> route -> TX -> per-rule stats
```

已经形成可复现闭环。

## 10. 边界

本项目不证明：

```text
真实 NIC 线速
真实 RSS 多队列
完整 rte_acl / rte_lpm library
生产级控制面热更新
```

可以继续扩展：

```text
hand-coded route_lookup()
  -> rte_lpm

hand-coded acl_drop()
  -> rte_acl

single queue
  -> RSS multi queue

net_null TX
  -> real NIC / vhost / tap
```

