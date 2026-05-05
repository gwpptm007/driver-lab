# 03_CORE_CONCEPTS_AND_ARCHITECTURE

## 1. 为什么需要 DPDK 用户态数据面

传统 Linux kernel 网络路径存在以下开销：

```text
NIC 收包
  ↓
中断触发 (interrupt)
  ↓
内核驱动复制 (copy)
  ↓
skb 分配 (allocation)
  ↓
TCP/IP 协议栈处理 (protocol stack)
  ↓
socket API 复制到用户态 (copy to userspace)
```

**DPDK 用户态数据面的价值**：

- **零拷贝**：mbuf 直接在用户态访问网卡的 RX/TX 队列
- **零中断**：轮询模式（poll-mode），避免中断开销
- **零 syscall**：绕过内核，直接调用 DPDK API
- **批处理**：burst 模式减少 per-packet 开销

适合场景：
- 运营商媒体面（低延迟、高吞吐）
- 防火墙/负载均衡
- VNF（虚拟化网络功能）

---

## 2. DPDK 核心概念

### 2.1 EAL (Environment Abstraction Layer)

EAL 是 DPDK 对底层硬件/内核的抽象层，负责：

```c
rte_eal_init(argc, argv);  // DPDK 程序第一个调用
```

主要功能：
- 初始化 hugepages
- 管理 lcore（逻辑核心）
- 解析 `-l`（lcore）、`-n`（内存通道）、`-a`（PCI 设备）等参数
- 创建 multi-process 通信 socket

### 2.2 Hugepage

kernel 默认 page size = 4KB，访问效率低。

DPDK 使用 2MB hugepages：

```bash
# 配置 1024 个 2MB hugepages = 2GB
echo 1024 | sudo tee /proc/sys/vm/nr_hugepages
mount -t hugetlbfs hugetlbfs /mnt/huge
```

**优势**：
- 减少 TLB miss（2MB vs 4KB）
- 大块连续物理内存（mbuf pool 需要）
- 用户态直接访问，无需 kernel 介入

### 2.3 UIO / VFIO

用户态程序访问 PCI 设备的方式：

| 驱动 | 说明 | VMware 支持 |
|------|------|-------------|
| `uio_pci_generic` | kernel 内置，通用 UIO | ✅ 支持 |
| `vfio-pci` | 需要 IOMMU（VT-d）| ❌ VMware 不支持 |

当前测试机使用 `uio_pci_generic`。

### 2.4 PMD (Poll Mode Driver)

PMD 是 DPDK 的网卡驱动，工作在轮询模式：

```c
// 接收：轮询网卡 RX 队列
nb_rx = rte_eth_rx_burst(port, 0, pkts, burst_size);

// 发送：轮询发送
nb_tx = rte_eth_tx_burst(port, 0, pkts, nb_tx);
```

无需中断，每次调用直接检查硬件队列。

### 2.5 Mempool 和 Mbuf

**Mempool**：管理大量 mbuf 对象的内存池

```c
mbuf_pool = rte_pktmbuf_pool_create(
    "mbuf_pool",     // 名字
    8192,           // mbuf 数量
    250,            // per-lcore 缓存
    0,
    RTE_MBUF_DEFAULT_BUF_SIZE,  // 2176 字节
    rte_socket_id()            // NUMA socket
);
```

**Mbuf**：数据包缓冲区抽象

```c
struct rte_mbuf {
    void *buf_addr;      // 数据缓冲区地址
    uint16_t data_off;   // 数据起始偏移
    uint16_t pkt_len;    // 包长度
    struct rte_mempool *pool;  // 来自哪个 mempool
    // ... 其他字段
};
```

DPDK 程序不直接分配内存，而是从 mempool 获取 mbuf。

### 2.6 Port 和 Queue

**Port**：网络端口抽象，对应一个物理/虚拟网卡

```c
// 配置端口（1 个 RX 队列，1 个 TX 队列）
rte_eth_dev_configure(port_id, 1, 1, &port_conf);

// 设置 RX/TX 队列
rte_eth_rx_queue_setup(port_id, 0, 1024, socket_id, NULL, mbuf_pool);
rte_eth_tx_queue_setup(port_id, 0, 1024, socket_id, &txconf);

// 启动端口
rte_eth_dev_start(port_id);
```

### 2.7 vhost-user 和 virtio-user

**vhost-user**：DPDK 实现的后端 backend，通过 UNIX socket 与前端通信

```bash
--vdev=net_vhost0,iface=/tmp/sock,client=0
# server 模式：vhost-user 创建 socket
# client 模式：QEMU 创建 socket，DPDK 连接
```

**virtio-user**：DPDK 实现的前端 frontend，模拟 virtio 设备

```bash
--vdev=net_virtio_user0,path=/tmp/sock
# 连接到 vhost-user socket
```

### 2.8 testpmd

DPDK 官方测试工具，可用于：
- 验证网卡和 DPDK 环境
- 简单的收发包测试
- 学习 DPDK API

---

## 3. Kernel vs DPDK 路径对比

### 3.1 Kernel 网络路径

```text
NIC 硬件
    ↓
kernel 驱动 (e1000/vmxnet3)
    ↓
netdev 层 (struct net_device)
    ↓
NAPI 轮询 (或中断)
    ↓
sk_buff 分配和复制
    ↓
TCP/IP 协议栈
    ↓
socket API (recv/send)
    ↓
用户态应用
```

**特点**：
- 中断驱动（高延迟）
- 多次内存拷贝
- 复杂协议栈处理

### 3.2 DPDK 用户态路径

```text
NIC 硬件
    ↓
UIO/VFIO 映射
    ↓
PMD (Poll Mode Driver)
    ↓
mbuf (用户态直接访问)
    ↓
用户态应用 (l2fwd-lite / fastpath-lite)
    ↓
PMD TX
    ↓
UIO/VFIO 映射
    ↓
NIC 硬件
```

**特点**：
- 轮询模式（零中断）
- 零拷贝（mbuf 直接访问）
- 可选协议处理（可完全绕过 TCP/IP）

### 3.3 数据流对比

```
Kernel:         NIC → skb → skb_copy → user buffer → app
                                    ↑ 拷贝

DPDK:           NIC → mbuf → app (直接访问 mbuf->buf_addr)
                                    ↑ 无拷贝
```

---

## 4. 学习顺序建议

不要先写复杂 C 代码。先跑通这个路径：

```
hugepage 配置
    ↓
uio_pci_generic bind
    ↓
testpmd 验证
    ↓
vhost-user socket
    ↓
virtio-user 连接
    ↓
自写 L2 forwarding app
```

当前 track 的 5 个阶段就是按这个顺序设计的。

---

## 5. 关键 API 速查

| 功能 | API |
|------|-----|
| DPDK 初始化 | `rte_eal_init()` |
| 创建 mbuf 池 | `rte_pktmbuf_pool_create()` |
| 配置端口 | `rte_eth_dev_configure()` |
| 设置 RX 队列 | `rte_eth_rx_queue_setup()` |
| 设置 TX 队列 | `rte_eth_tx_queue_setup()` |
| 启动端口 | `rte_eth_dev_start()` |
| 接收包 | `rte_eth_rx_burst()` |
| 发送包 | `rte_eth_tx_burst()` |
| 获取统计 | `rte_eth_stats_get()` |
| 停止端口 | `rte_eth_dev_stop()` |
| 清理 DPDK | `rte_eal_cleanup()` |