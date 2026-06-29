# 04_ARCHITECTURE_PRINCIPLES - DPDK Advanced 鍘熺悊鎬诲浘

> 杩欑瘒鏄?`track-dpdk-advanced` 鐨勬€诲師鐞嗗浘銆傚畠鐢?Mermaid 鎶婂綋鍓嶉」鐩殑鍒嗗眰銆佹暟鎹矾寰勩€佸疄楠屽叧绯汇€乵buf 鐢熷懡鍛ㄦ湡銆丷SS 杈圭晫銆乂FIO/IOMMU 杈圭晫鍜?L3 forwarder lite 涓茶捣鏉ャ€?
## 1. Track 鎬讳綋鑳藉姏鍦板浘

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

## 2. 椤圭洰鍒嗗眰鏋舵瀯

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

杩欏紶鍥惧搴旈」鐩爣鍑嗕氦浠樻柟寮忥細

```text
浠ｇ爜鎴栬剼鏈?-> 鐞嗚В鏂囨。 -> 娴嬭瘯鍛戒护 -> records 璇佹嵁 -> reports 鎶ュ憡
```

## 3. DPDK 鍩虹鏁版嵁璺緞

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

鏍稿績鐐癸細

- PMD 鎵归噺鏀跺寘锛屽簲鐢ㄦ嬁鍒扮殑鏄?`struct rte_mbuf *`銆?- 搴旂敤澶勭悊鍚庤涔?TX锛岃涔?free銆?- `tx_burst()` 鎴愬姛鍚庯紝mbuf 鎵€鏈夋潈浜ょ粰 PMD锛屽簲鐢ㄤ笉鑳界户缁闂€?
## 4. Phase 1: mbuf / mempool 鐢熷懡鍛ㄦ湡

```mermaid
stateDiagram-v2
    [*] --> FreeInMempool
    FreeInMempool --> AllocatedByPMD: RX needs buffer
    AllocatedByPMD --> FilledByPcapPMD: pcap packet copied
    FilledByPcapPMD --> OwnedByApp: rte_eth_rx_burst returns mbuf
    OwnedByApp --> MetadataRead: inspect pkt_len/data_len/data_off
    MetadataRead --> ReturnedToMempool: rte_pktmbuf_free
    ReturnedToMempool --> FreeInMempool
```

Phase 1 鐨勫師鐞嗘槸瑙傚療锛屼笉鏄浆鍙戯細

```text
pcap PMD -> mbuf metadata -> free mbuf
```

鍏抽敭璇佹嵁锛?
```text
data_off=128
pkt_len=67
data_len=67
nb_segs=1
software_rx_packets=32
```

## 5. Phase 2: RSS / multi-queue 鑳藉姏鍒ゆ柇

```mermaid
flowchart TD
    DevInfo["rte_eth_dev_info_get()"] --> Cap{"PMD capability"}
    Cap --> Q["max_rx_queues"]
    Cap --> R["reta_size"]
    Cap --> O["rss_offloads"]
    Q --> Check{"enough queues?"}
    R --> Check
    O --> Check
    Check -->|yes| RSS["configure RSS + queue map"]
    Check -->|no| Blocked["BLOCKED_PCAP_RSS"]
    RSS --> Map["rxq -> lcore"]
    Blocked --> Doc["record boundary evidence"]
```

鏈」鐩綋鍓嶇粨鏋滐細

```text
driver_name=net_pcap
max_rx_queues=1
reta_size=0
rss_offloads=0x0
```

鎵€浠?Phase 2 姝ｇ‘缁撹鏄?boundary锛屼笉鏄け璐ャ€?
## 6. Phase 3: burst/cache 璋冧紭鐭╅樀

```mermaid
flowchart LR
    B["burst list: 1,4,16,32,64"] --> Matrix["test matrix"]
    C["cache list: 0,64,250"] --> Matrix
    Matrix --> Run["run pcap drain"]
    Run --> CSV["MATRIX.csv"]
    CSV --> Metrics["rx_packets / duration / pps / polls"]
    Metrics --> Summary["PASS_TUNING_METHOD"]
```

璋冧紭鏂规硶鐨勫叧閿槸鎺у埗鍙橀噺锛?
```text
鍚屼竴 pcap
鍚屼竴 lcore 鍙傛暟
鍚屼竴 DPDK app
鍙敼鍙?burst_size 鍜?mempool_cache
```

## 7. Phase 4: UIO / VFIO / IOMMU 杈圭晫

```mermaid
flowchart TB
    NIC["PCI NIC / vmxnet3"] --> Driver{"driver binding"}
    Driver --> Kernel["kernel driver: vmxnet3/e1000"]
    Driver --> UIO["uio_pci_generic"]
    Driver --> VFIO["vfio-pci"]

    UIO --> UIOBoundary["userspace BAR access\nno strong IOMMU isolation"]
    VFIO --> IOMMU{"IOMMU groups exist?"}
    IOMMU -->|yes| Safe["VFIO isolation path possible"]
    IOMMU -->|no| Block["checklist only\nno real VFIO claim"]

    Kernel --> Mgmt["management NIC must stay safe"]
```

褰撳墠娴嬭瘯鏈轰簨瀹烇細

```text
iommu_group_entries=0
vfio_module_loaded=no
uio_module_loaded=no
ens192 uses vmxnet3 kernel driver
```

鎵€浠?Phase 4 鐨勫師鐞嗘槸鈥滈儴缃茶竟鐣屽彇璇佲€濓紝涓嶆槸鈥滃己琛屽垏 VFIO鈥濄€?
## 8. Phase 5: L3 forwarder lite 娴佺▼鍥?
```mermaid
flowchart TD
    RX["net_pcap0 RX"] --> Burst["rte_eth_rx_burst(port0)"]
    Burst --> Eth{"Ethernet type IPv4?"}
    Eth -->|no| NonIPv4["non_ipv4_drops++\nfree mbuf"]
    Eth -->|yes| IP{"IPv4 header valid?"}
    IP -->|no| ParseDrop["parse_drops++\nfree mbuf"]
    IP -->|yes| UDP{"protocol UDP?"}
    UDP -->|no| ParseDrop
    UDP -->|yes| ACL{"udp dst port == 9999?"}
    ACL -->|yes| ACLDrop["acl_drops++\nACL_STATS[0]++\nfree mbuf"]
    ACL -->|no| Route{"dst in 10.20.0.0/24?"}
    Route -->|no| Miss["route_miss_drops++\nfree mbuf"]
    Route -->|yes| TX["rte_eth_tx_burst(port1)"]
    TX --> OK{"sent == 1?"}
    OK -->|yes| Fwd["forwarded_packets++\nROUTE_STATS[0]++"]
    OK -->|no| TxFail["tx_failed++\nfree mbuf"]
```

鏈娴嬭瘯娴侀噺锛?
```text
48 packets total
24 forward
12 ACL drop
12 route miss
0 tx_failed
```

## 9. Phase 5 sequence diagram

```mermaid
sequenceDiagram
    participant Script as scripts/02_run_pcap_l3_forward.sh
    participant Pcap as tools/gen_l3_pcap.py
    participant EAL as DPDK EAL
    participant App as dpdk-l3-forwarder-lite
    participant RX as net_pcap0
    participant TX as net_null1
    participant Rec as records/

    Script->>Pcap: generate 48 UDP packets
    Pcap-->>Rec: l3_input.pcap
    Script->>App: start with --vdev net_pcap0 --vdev net_null1
    App->>EAL: rte_eal_init()
    App->>RX: setup RX queue
    App->>TX: setup TX queue
    loop until pcap drained
        App->>RX: rte_eth_rx_burst()
        RX-->>App: rte_mbuf packets
        App->>App: parse Ethernet/IPv4/UDP
        App->>App: ACL then route lookup
        App->>TX: rte_eth_tx_burst() for route hit
    end
    App-->>Rec: RESULT / ROUTE_STATS / ACL_STATS
```

## 10. Phase 5 class diagram

```mermaid
classDiagram
    class app_config {
        uint32_t nb_mbuf
        uint32_t mbuf_cache
        uint16_t burst_size
        uint16_t in_port
        uint16_t out_port
        route_rule routes[]
        acl_rule acl[]
    }

    class route_rule {
        uint32_t prefix_be
        uint32_t mask_be
        uint8_t prefix_len
        uint16_t out_port
        uint64_t hits
        uint64_t bytes
    }

    class acl_rule {
        uint16_t udp_dst_port
        uint64_t drops
        uint64_t bytes
    }

    class app_stats {
        uint64_t rx_packets
        uint64_t forwarded_packets
        uint64_t acl_drops
        uint64_t route_miss_drops
        uint64_t tx_failed
    }

    app_config "1" --> "*" route_rule
    app_config "1" --> "*" acl_rule
    app_config "1" --> "1" app_stats
```

杩欏紶 UML 鍥惧搴?`project-dpdk-l3-forwarder-lite/app/main.c` 鐨勬牳蹇冪粨鏋勩€?
## 11. 娴嬭瘯璁板綍閾捐矾

```mermaid
flowchart LR
    Env["00_check_env.sh"] --> EnvLog["ENV_CHECK.log"]
    Build["01_build.sh"] --> BuildLog["BUILD.log"]
    Run["02_run_*.sh"] --> RunLog["RUN.log"]
    Run --> Pcap["generated pcap"]
    Collect["03_collect_report.sh"] --> Summary["SUMMARY.md"]
    EnvLog --> Summary
    BuildLog --> Summary
    RunLog --> Summary
    Pcap --> Summary
```

姣忎釜闃舵閮戒繚鎸佺浉鍚岃瘉鎹摼锛?
```text
env -> build -> run -> summary -> report
```

## 12. 鏈€缁堣竟鐣屾ā鍨?
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

    B2 --> Need1["needs RSS-capable NIC/PMD"]
    B4 --> Need2["needs IOMMU groups and safe NIC"]
    B5 --> Need3["needs hardware traffic generator or real NIC pair"]
```

杩欎釜鍥炬槸鏈€缁堥潰璇曞彛寰勶細

```text
鑳借瘉鏄庣殑灏辩粰 records锛?鐜涓嶈兘璇佹槑鐨勫氨鍐?boundary锛?涓嶆妸妯℃嫙鐜鍖呰鎴愮敓浜х‖浠剁粨鏋溿€?```
