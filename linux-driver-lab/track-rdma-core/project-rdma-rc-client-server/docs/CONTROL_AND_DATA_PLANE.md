# CONTROL_AND_DATA_PLANE

## TCP 控制面

TCP 控制面只交换连接 RDMA RC QP 需要的元数据，不承载业务数据。

```mermaid
sequenceDiagram
    participant S as rdma-rc-server
    participant C as rdma-rc-client
    S->>S: prepare local metadata
    C->>C: prepare local metadata
    C->>S: connect 127.0.0.1:18515
    S-->>C: role/qpn/psn/gid/addr/rkey
    C-->>S: role/qpn/psn/gid/addr/rkey
```

元数据格式：

```text
role=server qpn=123 psn=0x111111 gid_index=1 gid=00000000000000000000000000000000 addr=0x12345678 rkey=0xabcdef01
```

## 数据面后续阶段

```mermaid
flowchart LR
    Meta["TCP metadata"] --> RTS["QP RESET/INIT/RTR/RTS"]
    RTS --> SR["RC SEND/RECV"]
    SR --> W["RDMA WRITE"]
    W --> R["RDMA READ"]
    R --> E["wrong rkey / CQE error"]
```

RDMA 数据面不会通过 TCP 搬运 payload。TCP 只是让双方知道对方的 QPN、PSN、GID 和 MR 授权信息。

更详细的原理、状态机和测试命令见：

```text
DEEP_LEARNING.md
TEST_FLOW.md
```
