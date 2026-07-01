# RDMA Interview Notes

## 三分钟项目介绍

我用 rdma-core 和 Soft-RoCE 构建了一条从 verbs 对象到数据传输的递进实验路线。先验证 device/context/PD/MR/CQ/QP 生命周期，再分别研究 MR 权限和 lkey/rkey；随后创建两个 RC QP，完成 RESET 到 RTS，并实现 SEND/RECV ping-pong。之后改为 one-sided RDMA WRITE/READ，观察远端无需 post receive 的语义，最后完成 UD datagram、AH、Q_Key 和 GRH offset 验证。

实验不是只看程序退出码。我记录每个 WR 的 wr_id、CQE status/opcode/byte_len，并校验实际 buffer。调试中还定位过 GID 与网卡 IPv6 地址不一致导致 RTR 超时，以及地址仍处于 DAD tentative 时路径不可用的问题。

## 高频追问

### PD 有什么用？

PD 是 QP 与 MR 的资源隔离边界。QP 使用的 lkey/rkey 必须与其 PD 关系有效，不能只靠进程拥有虚拟地址就访问任意 MR。

### lkey 和 rkey 分别给谁？

lkey 放在本地 SGE 中，授权设备访问本地 MR。rkey 与远端虚拟地址一起交给对端，用于 RDMA READ/WRITE 授权。

### SEND/RECV 与 WRITE 有什么区别？

SEND 需要接收端预投 Receive WR，双方都有 CQE。WRITE 直接修改远端授权 MR，远端不需要为该请求 post receive，也没有远端 RECV CQE。

### 为什么 QP 要经过 INIT/RTR/RTS？

INIT 配本地 port/P_Key/access；RTR 配对端 QPN、GID、PSN、MTU；RTS 配本地发送 PSN、timeout/retry。RC transport 需要双方状态和序列号一致。

### CQE success 是否代表业务成功？

不完全。它表示 verbs 操作完成且 status 成功，应用仍需验证 payload、协议状态和业务事务。

### Soft-RoCE 的局限？

能验证 verbs、状态机和协议语义；不能证明真实 RNIC offload、PCIe DMA、PFC/ECN、NUMA 和性能。

## 可讲的故障案例

QP 在 INIT -> RTR 返回 timeout。通过对比 `ip -6 addr show ens34` 与 `ibv_devinfo -v` 的 GID 表，发现选择的 GID 没有对应当前有效 netdev 地址；固定 `fe80::34/64` 并等待 DAD 完成后恢复。这个案例说明 RoCE 故障要跨 verbs、GID、IP 和 netdev 分层定位。
