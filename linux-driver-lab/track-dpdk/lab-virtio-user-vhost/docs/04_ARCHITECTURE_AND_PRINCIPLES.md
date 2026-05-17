# 04_ARCHITECTURE_AND_PRINCIPLES

## 概述

本实验验证 **DPDK virtio-user frontend 与 vhost-user backend 的本机互联闭环**。

```text
frontend testpmd                          backend testpmd
┌─────────────────────┐                  ┌─────────────────────┐
│  net_virtio_user0   │                  │   net_vhost0        │
│  (virtio-user PMD)  │                  │  (vhost-user PMD)   │
│         │           │                  │         │           │
│    txonly 模式      │                  │    rxonly 模式       │
│         │           │   UDS socket    │         │           │
│    发送数据包       │◄────────────────►│    接收数据包        │
└─────────────────────┘  /tmp/vhost.sock└─────────────────────┘
```

## 核心概念：virtio-user vs vhost-user

| 组件 | 角色 | DPDK PMD | 说明 |
|------|------|----------|------|
| **virtio-user** | Frontend（前端） | `net_virtio_user` | 扮演 virtio 设备的驱动端，发送数据 |
| **vhost-user** | Backend（后端） | `net_vhost` | 扮演 virtio 设备的接收端，接收数据并处理 |

这不是两个独立的东西，而是 **virtio 协议的两端**：

```text
传统 virtio 架构（QEMU/KVM）:
┌──────────────┐         ┌──────────────┐
│  virtio-net │  virtio  │   vhost-net  │
│  (Guest VM) │◄─────────►│   (QEMU/KVM) │
└──────────────┘          └──────────────┘

DPDK 纯用户态版本:
┌──────────────────┐      UDS      ┌──────────────────┐
│  virtio-user    │◄─────────────►│   vhost-user    │
│  (本实验前端)    │               │  (本实验后端)    │
└──────────────────┘               └──────────────────┘
```

## Frontend: net_virtio_user0 详解

### 是什么

`net_virtio_user` 是 DPDK 的 virtio-user PMD（Poll Mode Driver）。它模拟了一个 virtio 网络设备，但运行在**用户态**，不需要真实的网卡或 VM。

### 工作模式

```bash
--vdev=net_virtio_user0,path=/tmp/dpdk-vhost-user0,queues=1
```

| 参数 | 含义 |
|------|------|
| `net_virtio_user0` | 设备名称，DPDK 内部接口名 |
| `path=/tmp/dpdk-vhost-user0` | 连接到的 UDS socket 路径 |
| `queues=1` | virtqueue 数量（1 = 1 个 Rx + 1 个 Tx） |

### Frontend 发送流程（txonly 模式）

```
应用 → tx_burst() → virtio_user_tx() → UDS socket → backend
```

1. `testpmd` 在 `txonly` 模式下，不断从 mbuf pool 取包
2. 调用 `virtio_user_tx()` 将数据写入 virtqueue
3. 通过 UDS socket 通知 backend 数据可用
4. backend 通过 socket 读取数据

### 什么是 virtqueue

virtqueue 是 virtio 协议的核心数据结构，是一段**共享内存**：

```text
virtqueue 结构:
┌─────────────────────────────────────┐
│  descriptor table（描述符表）        │
│  - 描述每个 buffer 的地址/长度/标志  │
├─────────────────────────────────────┤
│  available ring（前端可写）         │
│  - 告诉后端哪些 buffer 有数据        │
├─────────────────────────────────────┤
│  used ring（后端可写）              │
│  - 告诉前端哪些 buffer 已处理完       │
└─────────────────────────────────────┘
```

virtqueue 通过 UDS 传递的**文件描述符**实现跨进程共享。

## Backend: net_vhost0 详解

### 是什么

`net_vhost` 是 DPDK 的 vhost-user PMD。它实现了 vhost-user 协议的 backend 端，负责接收来自 virtio-user 的数据包。

### 工作模式

```bash
--vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0
```

| 参数 | 含义 |
|------|------|
| `net_vhost0` | 设备名称 |
| `iface=/tmp/dpdk-vhost-user0` | 创建的 UDS socket 路径 |
| `queues=1` | vring 数量 |
| `client=0` | server 模式（DPDK 创建 socket） |

### Backend 接收流程（rxonly 模式）

```
frontend → UDS socket → vhost_user_rx() → rx_burst() → 应用
```

1. `testpmd` 在 `rxonly` 模式下不断轮询
2. 通过 UDS socket 接收 frontend 的数据包
3. 将数据放入 mbuf，传递给应用处理
4. 写 used ring 通知 frontend  buffer 已释放

## 通信流程深度分析

### 1. 连接建立（Handshake）

```text
backend 启动                    frontend 启动
     │                              │
     │ 创建 UDS socket (LISTEN)      │
     │──────────────────────────────►│
     │                              │ 连接 socket
     │◄──────────────────────────────│
     │                              │
     │  交换 vring 内存地址 (via FD)  │  ← 关键：通过 UDS 传递文件描述符
     │◄──────────────────────────────│
     │                              │
     │        连接建立完成           │
```

**关键点**：backend 和 frontend 通过 UDS 交换共享内存的文件描述符（FD）。这使得两端可以直接读写同一块物理内存，实现**零拷贝**数据传递。

### 2. 数据包传输（Data Path）

```text
frontend (txonly)              backend (rxonly)
     │                              │
     │  1. 应用生成数据包             │
     │  2. 写入 available ring      │
     │  3. 通过 socket 发送通知      │
     │─────────────────────────────►│
     │                              │  4. 收到通知
     │                              │  5. 从 available ring 读走数据
     │                              │  6. 处理数据（转发/丢弃）
     │  7. 写入 used ring           │
     │◄─────────────────────────────│
     │  8. 收到完成通知             │
     │  9. 回收 mbuf                │
     │                              │
```

### 3. 为什么用 rxonly 和 txonly

| 模式 | 角色 | 职责 |
|------|------|------|
| **frontend txonly** | 只发 | 不断生成数据包，向 backend 发送 |
| **backend rxonly** | 只收 | 不断轮询接收，不主动发送 |

这样设计的原因：
- 测试目的：验证链路能通，不关心实际转发
- 简化调试：单方向数据流，易于定位问题
- **零 RX 零 TX 也可能发生**：如果两端的 mbuf pool/descriptor 还没准备好，或者协商时序问题，数据可能还没真正流动

### 4. 查看数据流是否真正发生

在 `TESTPMD_BACKEND.log` 和 `TESTPMD_FRONTEND.log` 中看：

```
Port 0:  RX packets=123, TX packets=0
```

| 场景 | 含义 |
|------|------|
| backend: RX > 0, frontend: TX > 0 | 数据正常流动 |
| backend: RX = 0, frontend: TX = 0 | socket 连接了，但数据还没开始流动（常见） |
| backend: RX = 0, frontend: TX > 0 | frontend 发了，但 backend 没收到（检查协商） |

## 关键参数详解

### vhost-user 参数（backend）

```bash
--vdev=net_vhost0,iface=${SOCKET},queues=${QUEUES},client=${MODE}
```

| 参数 | 值 | 说明 |
|------|-----|------|
| `iface` | `/tmp/dpdk-vhost-user0` | UDS socket 路径 |
| `queues` | `1` | vring 数量（通常设为 CPU 核数） |
| `client` | `0` | 0 = server 模式（DPDK 创建 socket）；1 = client 模式（QEMU 创建，DPDK 连接） |

### virtio-user 参数（frontend）

```bash
--vdev=net_virtio_user0,path=${SOCKET},queues=${QUEUES}
```

| 参数 | 值 | 说明 |
|------|-----|------|
| `path` | `/tmp/dpdk-vhost-user0` | 连接的 UDS socket 路径（与 backend 的 iface 相同） |
| `queues` | `1` | 必须与 backend 的 queues 相同 |

### 为什么要用不同的 file-prefix

```bash
--file-prefix=vhost_backend    # backend 用
--file-prefix=virtio_frontend # frontend 用
```

两个 testpmd 进程运行在同一台机器上，共享大页内存。使用不同 prefix 避免：
- hugepage 文件冲突
- runtime 目录冲突
- 内存池名称冲突

## 深度分析：为什么 UDS 适合这个场景

### UDS 的独特能力：传递文件描述符

在 vhost-user 协议中，backend 需要直接访问 frontend 的虚拟内存（用于零拷贝）。这通过 UDS 的 `SCM_RIGHTS` 机制实现：

```c
// 伪代码：backend 通过 UDS 接收 frontend 的内存 FD
recvmsg(fd, &msg, 0);
// msg.msg_control 中包含 frontend 共享内存的文件描述符
```

TCP/UDP Socket **无法**传递文件描述符，这是选择 UDS 的根本原因。

### 为什么不用 Raw Socket

| 特性 | UDS | Raw Socket |
|------|-----|------------|
| 传文件描述符 | ✓ 支持 | ✗ 不支持 |
| 跨进程共享内存 | ✓ 支持 | ✗ 不支持 |
| 走 TCP/IP 协议栈 | ✗ 不走 | ✓ 走 |
| 延迟 | 极低（内存拷贝） | 较高（协议栈开销） |
| 本机进程通信 | ✓ 最佳选择 | 可以但不适合 |

### 抽象套接字（Abstract Socket）能用吗

**不能**。本实验在同一台机器上运行两个 testpmd，理论上可以用抽象套接字（路径以 `@` 开头）。但抽象套接字受 Network Namespace 隔离限制，如果后续扩展到跨容器/跨 NetNS 场景会出问题。本实验使用文件路径式 UDS（`/tmp/dpdk-vhost-user0`），兼容性更好。

## 故障排查原理

### socket 创建成功但没有数据流动

可能原因：

1. **协商时序问题**：frontend 在 backend 准备好之前就开始发包
   - 解决：本实验有 `PAIR_WARMUP_SECONDS=6` 秒预热时间

2. **mbuf pool 不足**：txonly 发包太快，rxonly 处理不过来
   - 解决：增加 hugepage 大小，或减少发包速率

3. **virtqueue 满**：frontend 的 available ring 被占满，backend 没来得及消费
   - 解决：检查 backend 的 `Send-Q` 是否很大（`ss -xl` 输出）

### 如何通过日志判断问题

```bash
# backend 日志中搜索
grep -E "vhost|net_vhost|Port [0-9]" TESTPMD_BACKEND.log

# frontend 日志中搜索
grep -E "virtio_user|net_virtio|Port [0-9]" TESTPMD_FRONTEND.log
```

正常输出应该包含：
- `Configuring Port 0` 或 `Port 0:`
- `RX packets=` 或 `TX packets=`
- device 名称（`net_vhost0` 或 `net_virtio_user0`）
