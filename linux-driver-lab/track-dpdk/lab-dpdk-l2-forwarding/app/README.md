# app/ - l2fwd-lite

`l2fwd-lite` 是一个最小化的 DPDK L2 转发程序，专为学习目的设计。

## 1. 有什么用

本 app 从 `testpmd` 进阶到手写数据面，验证以下 DPDK 核心 API：

```
rte_eal_init           ← EAL 环境初始化
rte_pktmbuf_pool_create← 大页内存池（mbuf）
rte_eth_dev_count      ← 探测可用网卡
rte_eth_dev_configure  ← 配置端口
rte_eth_rx_queue_setup ← 配置接收队列
rte_eth_tx_queue_setup ← 配置发送队列
rte_eth_dev_start      ← 启动端口
rte_eth_rx_burst       ← 接收数据包（轮询）
rte_eth_tx_burst       ← 发送数据包
rte_eth_stats_get      ← 获取网卡统计
rte_eth_dev_stop/close ← 停止/关闭端口
rte_eal_cleanup        ← 清理 EAL
```

## 2. 核心原理

### 2.1 DPDK 数据面架构

```
+------------------+     hugepages      +------------------+
|   User Space     |<------------------>|   Kernel         |
|                  |     (2MB pages)    |   (无直接访问)   |
| +--------------+ |                    | +--------------+ |
| |   l2fwd-lite | |                    | |   uio_pci    | |
| +--------------+ |                    | |   generic    | |
| |  mbuf pool   | |                    | +--------------+ |
| +--------------+ |                    +--------|---------+
| |  RX/TX queue | |<------ NIC ------->|        |         |
| +--------------+ |                    |    [ens192]       |
+------------------+                    +------------------+

DPDK 程序运行在用户态，通过 hugepages 获得大块连续物理内存，
绕过内核直接访问网卡的 TX/RX queue。
```

### 2.2 mbuf（报文缓冲区）

DPDK 使用 mbuf 作为数据包的内存抽象：

```c
struct rte_mbuf {
    struct rte_mbuf_pool *pool;  // 来自哪个内存池
    void *buf_addr;               // 数据缓冲区地址
    uint16_t data_off;            // 数据起始偏移
    uint16_t pkt_len;             // 包长度
    // ... 其他字段
};
```

- `rte_pktmbuf_pool_create`：创建内存池，分配 N 个 mbuf
- `rte_eth_rx_burst`：从 NIC 接收数据包到 mbuf
- `rte_eth_tx_burst`：从 mbuf 发送数据包到 NIC
- `rte_pktmbuf_free`：释放 mbuf 回内存池

### 2.3 端口配对（Port Pairing）

当有多个端口时，按 DPDK 枚举顺序两两配对：

```text
端口 0 <-> 端口 1
端口 2 <-> 端口 3
...
```

配对算法（`paired_port()`）：
```c
// 偶数索引 i → 配对到 i+1
// 奇数索引 i → 配对到 i-1
```

### 2.4 MAC 地址交换（L2 反向）

每次转发前交换以太网头的 src/dst MAC：

```c
// 原始:     dst=MAC_B  src=MAC_A
// 转发后:   dst=MAC_A  src=MAC_B
tmp = eth->src_addr;
eth->src_addr = eth->dst_addr;
eth->dst_addr = tmp;
```

这模拟了真实 L2 交换机的学习行为。

### 2.5 单端口 smoke 模式

当前 VMware 测试机只有一个 VMXNET3 网卡，无法形成端口配对。

程序检测到 `nb_ports_used < 2` 时：
- 仍然执行 RX burst 接收数据包
- 因为没有 peer 端口，调用 `rte_pktmbuf_free()` 释放 mbuf
- 记录 `no_peer_drop` 计数器

这不是失败，而是**当前硬件条件下的降级验证**。

## 3. 核心代码流程

```c
int main(int argc, char **argv)
{
    // 1. EAL 初始化（解析 -l -n 等 DPDK 参数）
    rte_eal_init(argc, argv);

    // 2. 创建 mbuf 内存池
    mbuf_pool = rte_pktmbuf_pool_create("l2fwd_lite_mbuf_pool",
                                        nb_mbuf, mbuf_cache, ...);

    // 3. 初始化所有可用端口
    init_all_ports(mbuf_pool);

    // 4. 转发主循环（轮询）
    forwarding_loop();

    // 5. 打印统计
    print_sw_stats();
    print_ethdev_stats();

    // 6. 停止端口并清理
    stop_all_ports();
    rte_eal_cleanup();
}
```

### 转发循环（`forwarding_loop`）

```c
while (!force_quit) {
    for (每个端口) {
        // RX：尝试从网卡接收数据包
        nb_rx = rte_eth_rx_burst(port, 0, pkts, burst_size);

        if (nb_rx > 0) {
            // 交换 MAC 地址
            for (每个包) {
                maybe_swap_eth_addr(pkts[j]);
            }

            // TX：发送到配对端口
            nb_tx = rte_eth_tx_burst(peer_port, 0, pkts, nb_rx);

            // 统计
            sw_stats[port].rx_packets += nb_rx;
            sw_stats[peer].tx_packets += nb_tx;
        }
    }

    // 定期打印软件统计
    if (时间到达) {
        print_sw_stats();
    }
}
```

## 4. 关键参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--nb-mbuf` | 8192 | mbuf 数量，越多越能应对突发 |
| `--burst-size` | 32 | 每次 RX/TX burst 的包数量 |
| `--rx-desc` | 1024 | RX 队列深度 |
| `--tx-desc` | 1024 | TX 队列深度 |
| `--run-seconds` | 15 | 运行时间，0 表示 Ctrl-C 退出 |
| `--stats-period` | 2 | 统计打印间隔（秒） |
| `--promisc` | 1 | 混杂模式，接收所有包 |

## 5. 编译

DPDK 21.11+ 使用 meson + ninja 构建：

```bash
cd app
meson setup build      # 配置（生成 build.ninja）
ninja -C build         # 编译
```

编译产物：`app/build/l2fwd-lite`（约 210KB）

## 6. 运行

### 单端口 smoke（当前测试机）

```bash
sudo ./build/l2fwd-lite -l 0-1 -n 4 \
  --file-prefix=l2fwd_lite \
  -a 0000:0b:00.0 \
  -- \
  --run-seconds 15 --stats-period 2
```

### 双端口 L2 转发（需要两个 DPDK 网卡）

```bash
sudo ./build/l2fwd-lite -l 0-2 -n 4 \
  --file-prefix=l2fwd_lite \
  -a 0000:0b:00.0 -a 0000:13:00.0 \
  -- \
  --run-seconds 30 --stats-period 2
```

### 参数说明

| EAL 参数 | 说明 |
|----------|------|
| `-l 0-1` | 使用 lcore 0 和 1 |
| `-n 4` | 内存通道数 |
| `--file-prefix` | 区分多个 DPDK 进程 |
| `-a PCI` | 暴露 PCI 设备给 DPDK |

## 7. 与 testpmd 的区别

| 特性 | testpmd | l2fwd-lite |
|------|---------|------------|
| 源码规模 | 数千行 | ~520 行 |
| 功能 | 完整转发/测试工具 | 最小学习骨架 |
| 命令行 | 交互式/批处理 | 启动时一次性参数 |
| forward mode | io/mac/sse 等多种 | 单一 L2 MAC swap |
| 统计 | 丰富 | 基础 |

`l2fwd-lite` 故意比 `examples/l2fwd` 更小，只保留核心 API 调用，便于理解 DPDK 数据面的本质。

## 8. 下一步演进

本 app 是 `project-user-space-fastpath` 的起点，后续会演进为：

```
UDP-only filter         ← 只处理特定 UDP 流
ARP/IP/UDP header rewrite ← 修改报文头部
per-port/per-flow stats ← 精细化统计
control-plane config    ← 支持运行时配置
records/replay/report   ← 流量回放和报告
```