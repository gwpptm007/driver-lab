# 04_DEEP_LEARNING - DPDK Advanced Summary 深度学习

> Phase 6 不是单独的数据面程序，而是把整个 track 的能力、证据和边界收敛成可讲、可查、可复盘的工程材料。

## 1. Summary 项目的角色

```mermaid
flowchart TB
    P1["Phase 1\nmbuf/mempool"] --> Summary["project-dpdk-advanced-summary"]
    P2["Phase 2\nRSS boundary"] --> Summary
    P3["Phase 3\nburst/cache matrix"] --> Summary
    P4["Phase 4\nVFIO/IOMMU boundary"] --> Summary
    P5["Phase 5\nL3 forwarder lite"] --> Summary

    Summary --> Final["DPDK_ADVANCED_FINAL_REPORT.md"]
    Summary --> Evidence["EVIDENCE_INDEX.md"]
    Summary --> Checklist["TUNING_CHECKLIST.md"]
    Summary --> Interview["INTERVIEW_NOTES.md"]
    Summary --> Resume["RESUME_MATERIAL.md"]
    Summary --> RDMA["RDMA_TRANSITION_NOTES.md"]
```

它的目标不是再写一个 demo，而是把所有 demo 的含义整理成作品集。

## 2. Evidence-first 原则

```mermaid
flowchart LR
    Claim["claim / resume bullet"] --> Evidence{"has records?"}
    Evidence -->|yes| Use["can state as PASS"]
    Evidence -->|no| Boundary["state as boundary / future work"]
    Use --> Report["final report"]
    Boundary --> Report
```

本 track 的核心纪律：

```text
有记录的能力写 PASS；
环境不能证明的能力写 BLOCKED 或 boundary；
不把 pcap/VMware 结果包装成真实硬件线速。
```

## 3. 能力分层

```mermaid
mindmap
  root((DPDK Advanced Capability))
    Memory
      mbuf
      mempool
      cache
      lifecycle
    Queue
      RX queue
      RSS
      RETA
      lcore mapping
    Performance Method
      burst size
      cache matrix
      CPU record
      NUMA record
    Deployment Boundary
      UIO
      VFIO
      IOMMU group
      vmxnet3
    Data Plane Project
      IPv4 UDP parse
      ACL
      route
      stats
```

## 4. Track-level 时序

```mermaid
sequenceDiagram
    participant User as learner
    participant P1 as mbuf/mempool
    participant P2 as RSS probe
    participant P3 as tuning matrix
    participant P4 as VFIO boundary
    participant P5 as L3 forwarder
    participant P6 as summary

    User->>P1: learn packet memory model
    P1->>P2: ask how packets scale across queues
    P2->>P3: ask how parameters affect data path
    P3->>P4: ask deployment constraints
    P4->>P5: build small L3 data plane
    P5->>P6: collect evidence and interview story
```

## 5. PASS / BLOCKED 状态机

```mermaid
stateDiagram-v2
    [*] --> Planned
    Planned --> Implemented: code/scripts/docs
    Implemented --> Tested: remote run
    Tested --> PASS: evidence satisfies acceptance
    Tested --> BLOCKED: environment capability missing
    BLOCKED --> BoundaryDocumented: reason recorded
    PASS --> Reported
    BoundaryDocumented --> Reported
    Reported --> [*]
```

Phase 2 是典型 `BLOCKED -> BoundaryDocumented`：

```text
pcap PMD max_rx_queues=1, reta_size=0, rss_offloads=0x0
```

Phase 4 也是 boundary：

```text
iommu_group_entries=0, vfio_module_loaded=no
```

## 6. 最终报告结构 UML

```mermaid
classDiagram
    class FinalReport {
        phase table
        proved capabilities
        boundary statement
    }

    class EvidenceIndex {
        records path
        report path
        key evidence
    }

    class TuningChecklist {
        memory
        burst
        queue
        driver binding
    }

    class InterviewNotes {
        one-minute story
        deep-dive questions
    }

    class ResumeMaterial {
        english bullets
        chinese bullets
    }

    FinalReport --> EvidenceIndex
    FinalReport --> TuningChecklist
    FinalReport --> InterviewNotes
    FinalReport --> ResumeMaterial
```

## 7. RDMA 过渡关系

```mermaid
flowchart LR
    DPDK_MBUF["DPDK mbuf/mempool"] --> RDMA_MR["RDMA MR / registered memory"]
    DPDK_Q["RX/TX queue"] --> RDMA_QP["QP send/recv queue"]
    DPDK_POLL["poll mode"] --> RDMA_CQ["CQ polling"]
    DPDK_IOMMU["VFIO/IOMMU boundary"] --> RDMA_PIN["DMA isolation / memory pinning"]
    DPDK_STATS["per-rule stats"] --> RDMA_WC["work completion / counters"]
```

这就是为什么 DPDK Advanced 后面自然接 RDMA Core。

## 8. Summary 项目的边界

Summary 证明的是：

```text
整个 track 有完整证据链、解释材料和面试表达。
```

它不新增硬件能力：

```text
不会把 Phase 2 的 RSS boundary 变成 PASS RSS 硬件；
不会把 Phase 4 的 IOMMU checklist 变成 VFIO 已跑通；
不会把 Phase 5 的 pcap/net_null 变成真实 NIC 线速。
```

