# ARCHITECTURE

## 1. 总体架构

```mermaid
flowchart LR
    NIC[NIC / pcap PMD] --> RX[DPDK RX queue]
    RX --> PARSE[Ethernet/IPv4/UDP parser]
    PARSE --> COPY[copy payload to staging slot]
    COPY --> REQ[32-byte gateway_request]
    REQ --> RING[SPSC request ring]
    RING --> RDMA[RDMA worker]
    RDMA --> WR[encode header + post RDMA WRITE]
    WR --> CQ[CQ completion]
    CQ --> FREE[slot generation check + recycle]
    WR --> REMOTE[remote registered memory]
```

DPDK 和 RDMA 的共同边界不是 `rte_mbuf *`，而是 staging slot 加固定描述符。原因是 mbuf 可能分段、来自 DPDK mempool、未注册为 verbs MR，并且 TX/RX 生命周期由 PMD 约束。直接把 mbuf 虚拟地址交给 RDMA 会把两套所有权模型耦合在一起。

## 2. 模块 UML

```mermaid
classDiagram
    class GatewayRequest {
        uint64 request_id
        uint64 flow_hash
        uint32 slot_id
        uint32 generation
        uint16 payload_len
        uint16 ingress_port
        uint16 rx_queue
        uint8 opcode
        uint8 flags
    }
    class GatewayRequestRing {
        GatewayRequest entries[64]
        atomic_uint producer
        atomic_uint consumer
        enqueue()
        dequeue()
    }
    class GatewayStagingSlot {
        alignas(64) payload[2048]
    }
    class GatewaySlotMeta {
        uint32 generation
        uint16 payload_len
        phase
    }
    GatewayRequestRing o-- GatewayRequest
    GatewayRequest --> GatewayStagingSlot : slot_id
    GatewayRequest --> GatewaySlotMeta : slot_id + generation
```

## 3. 本地 descriptor 布局

| offset | 大小 | 字段 | 含义 |
|---:|---:|---|---|
| 0 | 8 | `request_id` | 端到端唯一请求号 |
| 8 | 8 | `flow_hash` | DPDK 分类结果/未来分片依据 |
| 16 | 4 | `slot_id` | 本地 staging 索引，不发送到远端 |
| 20 | 4 | `generation` | 防止旧 CQE 回收新一代 slot |
| 24 | 2 | `payload_len` | 1-2048 字节 |
| 26 | 2 | `ingress_port` | DPDK 输入端口 |
| 28 | 2 | `rx_queue` | DPDK 输入队列 |
| 30 | 1 | `opcode` | 当前仅 `RDMA_WRITE` |
| 31 | 1 | `flags` | 后续扩展位 |

`_Static_assert(sizeof(struct gateway_request) == 32)` 把 ABI 变化变成编译错误。descriptor 只在本机线程之间传递，可以使用主机字节序；远端记录必须走显式 wire 编码。

## 4. Wire header

40 字节 wire header 使用大端序，并包含 magic、version、header length 和 reserved 校验。它不使用 `packed struct`，而是逐字段编码，避免编译器 padding、未对齐访问和不同架构字节序问题。

```mermaid
flowchart LR
    M[0..3 magic] --> V[4..5 version]
    V --> H[6..7 header_len]
    H --> O[8 opcode]
    O --> F[9 flags]
    F --> P[10..11 ingress_port]
    P --> Q[12..13 rx_queue]
    Q --> Z[14..15 reserved]
    Z --> R[16..23 request_id]
    R --> X[24..31 flow_hash]
    X --> L[32..35 payload_len]
    L --> G[36..39 generation]
```

decoder 在访问数据面前拒绝错误 magic、version、header length、reserved、opcode、payload length 和短 buffer。Phase 1 用 6 个负向 case 验证这些边界。

## 5. Slot 生命周期

```mermaid
stateDiagram-v2
    [*] --> FREE
    FREE --> READY: DPDK copy payload / generation++
    READY --> INFLIGHT: descriptor dequeue / post WR
    INFLIGHT --> FREE: matching CQE(slot_id, generation)
    READY --> READY: stale generation / reject
    INFLIGHT --> INFLIGHT: stale CQE / reject
```

generation 是 slot 的逻辑时代号。假设 slot 5 的 generation 1 已完成并被复用为 generation 2，迟到的 generation 1 completion 必须返回 `-ESTALE`，否则会把仍在使用的 generation 2 错误标成 FREE。

## 6. 所有权时序

```mermaid
sequenceDiagram
    participant D as DPDK producer
    participant S as staging slot
    participant Q as SPSC ring
    participant R as RDMA consumer
    participant C as CQ
    D->>S: FREE -> READY, copy payload
    D->>Q: enqueue request(slot, generation)
    Note over D,Q: release producer index
    Q->>R: dequeue request
    Note over Q,R: acquire producer index
    R->>S: READY -> INFLIGHT
    R->>R: encode header and post WRITE
    C-->>R: CQE(request_id)
    R->>S: generation match, INFLIGHT -> FREE
```

producer 在写完 descriptor 后以 release 发布索引；consumer 以 acquire 读取索引，因此看见新 producer 值时也必须看见完整 descriptor。反方向用 consumer release 和 producer acquire 保证槽位回收可见。

## 7. Backpressure

```mermaid
flowchart TD
    P[DPDK packet] --> F{free staging slot?}
    F -->|no| B1[slot_exhausted + drop/backpressure]
    F -->|yes| Q{request ring full?}
    Q -->|yes| B2[ring_full + retain/recycle slot]
    Q -->|no| W[RDMA worker post WR]
    W --> C{send queue/CQ budget available?}
    C -->|no| B3[poll CQ before posting more]
    C -->|yes| I[INFLIGHT]
```

生产者不能无限排队。Phase 1 的 ring 满返回 `-ENOSPC`；后续 DPDK 集成需要把该返回值映射为显式 drop/backpressure 统计，并把尚未入队的 READY slot 安全回收。

## 8. 当前边界

Phase 1 只证明 contract、字节序、ring 和 slot 生命周期，不证明：

- DPDK pcap 或真实 NIC 已接入。
- staging region 已注册为 verbs MR。
- RDMA WRITE/CQE 已执行。
- Soft-RoCE 或 RNIC 端到端性能。

这些能力按 `ROADMAP.md` 分阶段加入，每阶段都保留独立 marker 和测试记录。

## 9. Phase 2：pcap ingress

Phase 2 使用 pcap PMD 提供 64 个确定性 Ethernet/IPv4 报文：每四包中三包 UDP、一包 ICMP。parser 使用 `rte_pktmbuf_read()`，因此即使协议头跨 mbuf segment 也能安全读取；只有完整 UDP payload 才能占用 staging slot。

```mermaid
flowchart TD
    RX[rte_eth_rx_burst] --> E{Ethernet header valid?}
    E -->|no| MAL[malformed++]
    E -->|yes| IP{IPv4 + IHL valid?}
    IP -->|no| MAL
    IP -->|yes| UDP{protocol == UDP?}
    UDP -->|no| UNSUP[unsupported++]
    UDP -->|yes| LEN{8 < UDP length <= 2056?}
    LEN -->|no| MAL
    LEN -->|yes| SLOT[prepare_next FREE slot]
    SLOT --> COPY[rte_pktmbuf_read payload into staging]
    COPY --> ENQ[enqueue gateway_request]
    ENQ --> FREE[rte_pktmbuf_free original mbuf]
```

### 9.1 Copy 与 mbuf 所有权

当前选择 bounded copy，而不是注册整个 DPDK mempool。copy 完成后 staging 拥有独立 payload，原 mbuf 在当前 RX 循环立即释放，不需要等 RDMA completion。代价是一次内存复制，收益是 PMD、mempool 和 verbs MR 生命周期完全解耦。

如果 slot 用尽，`gateway_slot_prepare_next()` 返回 `-ENOSPC`；如果 request ring 满，producer 调用 `gateway_slot_cancel_ready()` 撤销尚未发布的 READY slot，防止 backpressure 路径泄漏资源。

### 9.2 Mock RDMA 的准确边界

```mermaid
sequenceDiagram
    participant P as pcap PMD
    participant D as DPDK ingress
    participant S as staging/ring
    participant M as mock RDMA consumer
    P->>D: 64 mbufs
    D->>S: 48 UDP payloads + requests
    D->>D: free all 64 mbufs
    Note over D: 16 ICMP -> unsupported
    S->>M: dequeue 48 requests
    M->>M: wire encode/decode
    M->>S: 48 matching generation completions
```

mock consumer 证明 descriptor 到 staging 的关联、wire codec 和 completion 回收可以组合，但没有调用 `ibv_post_send()`。因此 Phase 2 marker 只命名为 `INGRESS_PASS`，RDMA backend 留给 Phase 3。

## 10. Phase 3：RXE RDMA backend

Phase 3 复用 one-sided KV 已验证的 `rdma_cs` 基础库完成 device/PD/MR/CQ/QP 生命周期与 TCP metadata exchange，gateway adapter 只负责把 request 和 staging payload 编成 remote record。复用的是底层 verbs 工程能力，不复用 KV 协议。

```mermaid
flowchart LR
    C[Gateway RDMA client] --> AD[gateway_rdma_backend]
    AD --> CS[rdma_cs resources/QP/control plane]
    CS --> RXE[rxe0 RC QP]
    RXE --> MR[Server remote MR 4096 bytes]
    AD --> REC[40-byte header + payload]
    REC --> WR[IBV_WR_RDMA_WRITE]
```

### 10.1 对象与建链顺序

```mermaid
sequenceDiagram
    participant C as client
    participant T as TCP control plane
    participant S as server
    C->>C: device -> context -> PD -> MR -> CQ -> QP
    S->>S: device -> context -> PD -> MR -> CQ -> QP
    C->>T: QPN/PSN/GID/addr/rkey
    S->>T: QPN/PSN/GID/addr/rkey
    T-->>C: server metadata
    T-->>S: client metadata
    C->>C: RESET -> INIT -> RTR -> RTS
    S->>S: RESET -> INIT -> RTR -> RTS
```

TCP 只承载 QP/MR metadata 和测试同步 token。`gateway_request` header 与 payload 不通过 TCP；它们由 client 的 registered send MR 发起 one-sided WRITE。

### 10.2 WRITE 与 completion

```mermaid
sequenceDiagram
    participant SLOT as staging slot
    participant C as client QP
    participant S as server remote MR
    participant CQ as client CQ
    SLOT->>SLOT: READY -> INFLIGHT
    SLOT->>C: encode 40-byte header + 32-byte payload
    C->>S: IBV_WR_RDMA_WRITE, 72 bytes
    CQ-->>C: wr_id=3001, IBV_WC_SUCCESS
    C->>SLOT: generation match, INFLIGHT -> FREE
    C-->>S: TCP WRITE_DONE，仅测试同步
    S->>S: decode header + compare payload
```

RC WRITE 的 client CQE 表示本次 signaled WR 成功完成；one-sided WRITE 不在 server CQ 生成接收 completion，所以测试使用 TCP token 通知 server 在 CQE 之后检查 remote MR。这个 token 不是业务数据面，也不能替代生产协议中的持久化或应用可见性确认。

### 10.3 Remote record 布局

```text
remote_addr + 0   : gateway wire header, 40 bytes
remote_addr + 40  : UDP/staging payload, payload_len bytes
```

本阶段写入 72 字节：40 字节 header 加 32 字节 payload。server 使用同一 wire decoder 验证 magic/version/request_id/generation/payload_len，再比较 payload 内容。

### 10.4 当前边界

- RXE/Soft-RoCE 证明 verbs 语义和工程流程，不代表 RNIC DMA/PCIe 性能。
- Phase 3 client 使用构造的 staging payload，尚未直接消费 Phase 2 pcap request ring。
- 每次只有一个 signaled WRITE；batch WR 与 selective signaling 留到 Phase 5。

## 11. Phase 4：DPDK 到 RDMA 端到端集成

Phase 4 将前两阶段的独立程序合并到同一个 client 进程。DPDK 主线程是 ring/slot 的唯一 producer；RDMA pthread 是唯一 consumer，并独占 QP、CQ 和 registered send MR。该边界让 DPDK 代码不依赖 verbs 对象，也避免两个线程并发 poll 同一个 CQ。

```mermaid
flowchart LR
    PCAP[pcap PMD: 64 packets] --> RX[DPDK RX main thread]
    RX -->|48 UDP| ST[staging slots]
    RX -->|publish release| R[SPSC request ring]
    RX -->|16 ICMP| DROP[unsupported]
    R -->|dequeue acquire| W[RDMA worker pthread]
    W --> ENC[wire header + payload]
    ENC --> QP[RC QP / signaled WRITE]
    QP --> RM[server remote MR]
    QP --> CQ[client CQE]
    CQ -->|generation match| FREE[slot FREE]
```

### 11.1 线程、内存序与停止协议

```mermaid
sequenceDiagram
    participant D as DPDK producer
    participant S as staging/ring
    participant W as RDMA worker
    participant Q as RC QP/CQ
    D->>S: copy UDP payload, slot FREE -> READY
    D->>S: enqueue descriptor (release)
    W->>S: dequeue descriptor (acquire)
    W->>S: READY -> INFLIGHT
    W->>Q: post signaled RDMA WRITE
    Q-->>W: IBV_WC_SUCCESS
    W->>S: complete matching generation, INFLIGHT -> FREE
    D->>W: stop=true (release)
    W->>S: continue until ring empty
    W-->>D: join after drain
```

`stop=true` 只表示不会再发布新 request，不表示 worker 可以立即退出。worker 必须同时观察到 stop 和 ring empty 才结束；主线程 `pthread_join()` 返回后，才可以断开 QP 并销毁 staging/ring。这个 drain 协议避免进程收尾时遗失已发布 descriptor。

### 11.2 守恒关系

确定性 pcap 每 4 包包含 3 个 UDP 和 1 个 ICMP，因此 64 包应满足：

```text
rx = udp + unsupported + malformed = 64
udp = staged = dequeued = completed = 48
payload_bytes = 48 * 32 = 1536
write_bytes = 48 * (40 + 32) = 3456
active_slots(after drain) = 0
```

server remote MR 只有一个 record 区域。RC QP 保序，client 在每个 signaled WRITE 得到成功 CQE 后再发下一个，所以 batch 结束后 server 应看到最后一条 request 48，payload 为 `GATEWAY_UDP_0062`。这验证了最终远端内容，但不是多槽远端队列或持久化协议。

### 11.3 错误传播与回收

```mermaid
stateDiagram-v2
    [*] --> Running
    Running --> Draining: producer stop
    Running --> Failed: parse/slot/ring/RDMA error
    Draining --> Complete: ring empty and all CQE handled
    Draining --> Failed: RDMA error
    Complete --> Cleanup
    Failed --> Cleanup
    Cleanup --> [*]
```

- ring 满时 producer 撤销 READY slot，不发布半成品 descriptor。
- WRITE 或 CQE 失败时 worker 增加 error 计数并停止正常验收，slot 不会被错误 generation 释放。
- 正常结束必须同时满足 `errors=0`、`completed=48`、`active_slots=0`。

### 11.4 当前环境边界

- DPDK pcap PMD 没有真实 NIC RX queue、RSS、DMA 和 PCIe 压力。
- RXE/Soft-RoCE 没有真实 RNIC doorbell、WQE cache 和 NUMA 路径。
- 当前每个 request 都是 signaled WR，便于证明 slot 回收正确；batch WR 和 selective signaling 属于后续性能扩展。
- Phase 1-4 已构成可解释、可回归的功能 capstone，状态为 `DPDK_RDMA_GATEWAY_CURRENT_ENV_COMPLETE`。
