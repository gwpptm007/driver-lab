# RDMA Resume Material

## 推荐项目标题

Linux RDMA Core / RoCEv2 数据路径实验平台

## RESUME总结描述

- 基于 C、rdma-core/libibverbs 与 Soft-RoCE 搭建 RDMA 递进实验平台，覆盖 device/context/PD/MR/CQ/QP 生命周期、RC/UD transport 和 RoCEv2 地址模型。
- 实现两个 RC QP 的 RESET/INIT/RTR/RTS 状态迁移及双向 SEND/RECV，解析并验证 CQE 的 wr_id、status、opcode、byte_len 与 payload。
- 实现 one-sided RDMA WRITE/READ，验证 remote address/rkey、MR access flags 和远端无需 post receive 的数据访问语义。
- 实现 UD datagram，验证 Address Handle、Q_Key、remote QPN 与 40-byte GRH 接收偏移。
- 建立自动测试和完整实测证据，定位 GID/netdev IPv6 不一致及 DAD tentative 导致的 RTR 超时问题。

## 不应夸大的表述

不要写“实现零拷贝高性能 RDMA 驱动”或“完成生产级 RNIC 优化”。当前证据来自 Soft-RoCE，证明的是 API、对象和 transport 语义，不包含硬件性能结论。
