# 04_ARCHITECTURE_PRINCIPLES - DPDK Advanced 原理总图

> 这篇是 `track-dpdk-advanced` 的总原理图。它用 Mermaid 串起项目分层、数据路径、mbuf 生命周期、RSS 边界、VFIO/IOMMU 边界和 L3 forwarder lite。

## 1. 能力地图

```mermaid
mindmap
  root((DPDK Advanced))
    Phase 1 mbuf/mempool
      mbuf metadata
      mempool cache
      pcap PMD RX
      stats consistency
    Phase 2 RSS/multiqueue
      PMD capability
      max_rx_queues
      reta_size
      rss_offloads
      queue-to-core model
      BLOCKED_PCAP_RSS
    Phase 3 tuning method
      burst size
      mempool cache size
      CPU record
      NUMA record
      matrix evidence
    Phase 4 VFIO/IOMMU boundary
      UIO
      VFIO
      IOMMU group
      vmxnet3 boundary
      no unsafe bind
    Phase 5 L3 forwarder lite
      IPv4/UDP parse
      ACL drop
      route lookup
      net_null TX
      per-rule stats
    Phase 6 summary
      final report
      evidence index
      interview notes
      resume material
      RDMA bridge
```

## 2. 项目分层

```mermaid
flowchart TB
    subgraph Docs["docs / reports"]
        D1["04_DEEP_LEARNING.md"]
        D2["02_TEST_AND_VERIFY.md"]
        D3["phase reports"]
        D4["final report / evidence index"]
    end

    subgraph Apps["DPDK C apps"]
        A1["dpdk-mbuf-inspect"]
        A2["dpdk-rss-queue-probe"]
        A3["dpdk-burst-cache-probe"]
        A4["dpdk-l3-forwarder-lite"]
    end

    subgraph Scripts["scripts"]
        S0["00_check_env.sh"]
        S1["01_build.sh"]
        S2["02_run_*.sh"]
        S3["03_collect_report.sh"]
    end

    subgraph Evidence["records"]
        R1["ENV_CHECK.log"]
        R2["BUILD.log"]
        R3["RUN.log"]
        R4["SUMMARY.md"]
        R5["pcap / csv"]
    end

    Scripts --> Apps
    Scripts --> Evidence
    Apps --> Evidence
    Evidence --> Docs
    Docs --> D4
```

## 3. DPDK 基础数据路径

```mermaid
flowchart LR
    P["PMD / ethdev port"] --> RXQ["RX queue"]
    RXQ --> Burst["rte_eth_rx_burst()"]
    Burst --> Mbuf["rte_mbuf"]
    Mbuf --> Parse["parse / inspect / classify"]
    Parse --> Decision{"action"}
    Decision -->|forward| TX["rte_eth_tx_burst()"]
    Decision -->|drop| Free["rte_pktmbuf_free()"]
    TX --> Sink["TX PMD / net_null / NIC"]
    Free --> Pool["mempool"]
    Pool --> RXQ
```

关键规则：

- 应用从 `rx_burst()` 拿到 mbuf 后拥有它。
- 如果不转发，应用必须调用 `rte_pktmbuf_free()`。
- 如果 `tx_burst()` 成功，mbuf 所有权交给 PMD，应用不能继续访问该 mbuf。

## 4. mbuf 生命周期

```mermaid
stateDiagram-v2
    [*] --> FreeInMempool
    FreeInMempool --> AllocatedByPMD: RX needs buffer
    AllocatedByPMD --> FilledByPcapPMD: pcap packet copied
    FilledByPcapPMD --> OwnedByApp: rx_burst returns mbuf
    OwnedByApp --> MetadataRead: inspect fields
    MetadataRead --> ReturnedToMempool: rte_pktmbuf_free
    ReturnedToMempool --> FreeInMempool
```

## 5. RSS capability 判断

```mermaid
flowchart TD
    DevInfo["rte_eth_dev_info_get()"] --> Cap{"PMD capability"}
    Cap --> Q["max_rx_queues"]
    Cap --> R["reta_size"]
    Cap --> O["rss_offloads"]
    Q --> Check{"enough capability?"}
    R --> Check
    O --> Check
    Check -->|yes| RSS["configure RSS + queue map"]
    Check -->|no| Blocked["BLOCKED_PCAP_RSS"]
    RSS --> Map["rxq -> lcore"]
    Blocked --> Doc["record boundary evidence"]
```

本项目当前结果：

```text
driver_name=net_pcap
max_rx_queues=1
reta_size=0
rss_offloads=0x0
```

## 6. VFIO / IOMMU 边界

```mermaid
flowchart TB
    NIC["PCI NIC / vmxnet3"] --> Driver{"driver binding"}
    Driver --> Kernel["kernel driver"]
    Driver --> UIO["uio_pci_generic"]
    Driver --> VFIO["vfio-pci"]

    UIO --> UIOBoundary["userspace BAR access\nno strong IOMMU isolation"]
    VFIO --> IOMMU{"IOMMU groups exist?"}
    IOMMU -->|yes| Safe["VFIO isolation path possible"]
    IOMMU -->|no| Block["checklist only\nno real VFIO claim"]

    Kernel --> Mgmt["management NIC must stay safe"]
```

当前测试机没有 IOMMU group，所以 Phase 4 只声明 boundary 和 checklist。

## 7. L3 forwarder lite 数据面

```mermaid
flowchart TD
    RX["net_pcap0 RX"] --> Burst["rte_eth_rx_burst(port0)"]
    Burst --> Eth{"Ethernet type IPv4?"}
    Eth -->|no| NonIPv4["non_ipv4_drops++"]
    Eth -->|yes| IP{"IPv4 header valid?"}
    IP -->|no| ParseDrop["parse_drops++"]
    IP -->|yes| UDP{"protocol UDP?"}
    UDP -->|no| ParseDrop
    UDP -->|yes| ACL{"udp dst port == 9999?"}
    ACL -->|yes| ACLDrop["ACL drop"]
    ACL -->|no| Route{"dst in 10.20.0.0/24?"}
    Route -->|no| Miss["route_miss_drops++"]
    Route -->|yes| TX["rte_eth_tx_burst(port1)"]
    TX --> Fwd["forwarded_packets++"]
```

## 8. L3 forwarder UML

```mermaid
classDiagram
    class app_config {
        nb_mbuf
        mbuf_cache
        burst_size
        routes[]
        acl[]
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
        tx_failed
    }

    app_config "1" --> "*" route_rule
    app_config "1" --> "*" acl_rule
    app_config "1" --> "1" app_stats
```

## 9. 最终边界模型

```mermaid
flowchart TD
    Result["track-dpdk-advanced"] --> Proved["proved in current env"]
    Result --> Boundary["kept as boundary"]

    Proved --> P1["mbuf/mempool metadata"]
    Proved --> P3["burst/cache matrix method"]
    Proved --> P5["L3 parse/ACL/route/stats"]

    Boundary --> B2["real RSS multiqueue"]
    Boundary --> B4["real VFIO/IOMMU binding"]
    Boundary --> B5["real NIC line-rate forwarding"]
```

最终口径：

```text
能证明的给 records。
环境不能证明的写 boundary。
不把模拟环境包装成生产硬件结果。
```

