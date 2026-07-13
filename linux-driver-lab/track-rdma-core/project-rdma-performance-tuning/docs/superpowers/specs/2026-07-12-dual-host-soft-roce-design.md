# Dual-Host Soft-RoCE Design

## 1. 目标

把 `project-rdma-performance-tuning` 从单机 `127.0.0.1 + rxe0/ens34` 迁到双机 Soft-RoCE 路径，先复用 `project-rdma-rc-client-server` 已验证过的 `134 -> 135 / ens33 / GID[1]` 约定。

本轮要完成：

- 双机运行脚本
- 双机 completion latency 迁移
- 双机 RTT/request-response latency 第一版
- 让现有 batch / inline / selective / poll budget 可以沿用双机脚本

## 2. 边界

本轮不做：

- 真实 RNIC
- NUMA / affinity
- event-driven CQ
- RTT 的 batch/selective 复杂矩阵

## 3. 方案

### 3.1 双机脚本层

直接复用上一项目结论：

- server 在 `135`
- client 在 `134`
- 两端 `rxe0` 都绑定到 `ens33`
- 使用 `gid_index=1` 的 IPv4-mapped GID

新增 `dual-server` / `dual-client` 脚本到 perf 项目中，但延续原有环境变量命名，避免重新学习一套入口。

### 3.2 数据面

completion latency 路径不改测量边界：

- client 仍测 `post_send -> local SEND CQE`

RTT 路径新增一段 request/response phase：

- server 先 post RECV 等待 request
- client 先 post RECV 等待 response
- client 发 request SEND
- server 收到 request 后发 response SEND
- client 收到 response RECV CQE 后记 RTT

TCP 只负责 phase 边界同步，不进入 RTT 计时窗口。

### 3.3 模式复用

现有这些变量继续生效：

- `PERF_ITERATIONS`
- `PERF_BATCH_SIZE`
- `PERF_USE_INLINE`
- `PERF_SIGNAL_INTERVAL`
- `PERF_POLL_CQ_BUDGET`

新增：

- `PERF_ENABLE_RTT=1`

这样 dual-client 在双机下仍可复用 completion 的 full mode，而 RTT 第一版先只覆盖 single request-response。
