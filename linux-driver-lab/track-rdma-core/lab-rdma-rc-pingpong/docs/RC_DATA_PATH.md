# RC Send/Recv 数据路径

```mermaid
sequenceDiagram
    participant Left
    participant LeftSQ
    participant RightRQ
    participant CQ
    Left->>RightRQ: post RECV before traffic
    Left->>LeftSQ: post SEND with SGE/lkey
    LeftSQ->>RightRQ: reliable transport
    RightRQ->>CQ: RECV CQE
    LeftSQ->>CQ: SEND CQE
    Left->>CQ: poll two completions
```

Receive WR 必须提前进入 RQ。否则发送端可能遇到 Receiver Not Ready，并依赖 RNR retry。

```mermaid
classDiagram
    class WorkRequest { wr_id opcode SGE }
    class SGE { address length lkey }
    class QueuePair { SQ RQ state=RTS }
    class CompletionEntry { wr_id status opcode byte_len }
    WorkRequest --> SGE
    WorkRequest --> QueuePair
    QueuePair --> CompletionEntry
```

- `wr_id` 由应用定义，用于把 CQE 关联回业务请求。
- SGE 的 lkey 授权本地设备访问已注册 MR。
- SEND CQE 的 `byte_len` 通常不用于表达发送长度；RECV CQE 的 `byte_len` 表示收到的字节数。
- `IBV_SEND_SIGNALED` 要求为 SEND 生成 CQE。

```mermaid
flowchart LR
    Ping["left SEND"] --> Right["right RECV buffer"]
    Right --> Verify1["ping payload verify"]
    Verify1 --> Pong["right SEND"]
    Pong --> Left["left RECV buffer"]
    Left --> Verify2["pong payload verify"]
```

当前实验在同一进程中使用两个 QP，已经经过 RXE transport 和 CQ 语义，但没有验证跨主机网络、丢包、拥塞或真实 RNIC 性能。
