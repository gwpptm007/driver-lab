# stage05 深度指南 — virtio-net 源码阅读 / 平台参数化

## 一、stage05 在整个学习路径中的位置

stage05 是 W5（DMA + performance）的第五天，承接 stage04 的 ring/DMA/RX replenishment，引入**真实 virtio-net 源码阅读**和**平台参数化**。

```
W5: DMA + performance (day29-35)
├── day29: DMA 基础 (dma_alloc_coherent / dma_map_page)
├── day30: mmap 零拷贝
├── day31: benchmarking 吞吐量 / 延迟
├── day32: perf / ftrace 性能分析
├── day33: stage04 引入 ring + DMA + RX replenishment
├── day34: 稳定性 + 回归测试
├── day35: stage05 virtio-net 源码阅读 + 平台参数化   ← 今天
└── day36+: stage06 ARM64 迁移
```

**stage05 的本质**：不写新代码，而是做一次**认知升级**——把 stage04 的"教学型简化模型"对照到真实 virtio-net 的"分层实现"。

---

## 二、stage05 的两个目标

### 目标 1：stage04 ↔ virtio-net 职责对照

| stage04 教学概念 | virtio-net / vring 对应层 | 说明 |
|-----------------|--------------------------|------|
| `tx_ring / rx_ring` | `send / receive virtqueue` | 都是在表达 buffer 提交与完成 |
| `owner/state` 显式字段 | `avail / used ring` 协议 | 教学用字段，真实用 ring 协议 |
| `raise_irq + napi_schedule` | `callback / napi schedule` | 都是在表达完成事件驱动 poll |
| `refill_rx_slot` | `try_fill_recv` | 都是在解决 RX buffer 不断粮 |
| `dma_map_single/unmap` 显式路径 | transport + sg + DMA 抽象 | 真实 virtio 有分层传输抽象 |

### 目标 2：平台参数化

从 stage05 起，统一环境变量抽象，支持多平台：

| 环境变量 | 说明 | host | arm64+qemu |
|---------|------|------|------------|
| `TARGET_ARCH` | 目标架构 | `host` | `arm64` |
| `RUN_MODE` | 运行模式 | `host` | `qemu-arm64` |
| `CROSS_COMPILE` | 交叉编译器前缀 | (空) | `aarch64-linux-gnu-` |
| `QEMU_BIN` | QEMU 二进制 | (空) | `qemu-system-aarch64` |
| `KERNEL_SOURCE_ROOT` | 内核源码根目录 | 需设置 | 需设置 |
| `KERNEL_BUILD_DIR` | 内核构建目录 | 需设置 | 需设置 |
| `VIRTIO_NET_SOURCE` | virtio_net.c 路径 | 需设置 | 需设置 |

---

## 三、virtio-net 源码阅读地图

### 3.1 推荐的阅读顺序

```
1. include/uapi/linux/virtio_net.h      ← 先看接口定义（命令字、特性位、头部结构）
2. drivers/virtio/virtio_ring.c          ← 再看 vring 实现（avail/used/direct）
3. drivers/net/virtio_net.c              ← 最后看网络设备实现（virtnet_* 函数群）
```

### 3.2 virtio_net.c 关键函数（按调用路径）

**初始化路径（probe → open）**：
```
virtnet_probe()
  ├─ virtnet_find_vqs()           → 分配和初始化 virtqueue
  ├─ virtnet_setup韬ngs()          → 分配 send/receive virtqueue
  ├─ try_fill_recv()              → 预填充 RX buffer（类似 stage04 refill）
  ├─ register_netdev()            → 注册 netdev
  └─ virtnet_ndo_set_rx_mode()   → 设置 MAC 过滤

virtnet_open()
  ├─ virtnet_napi_enable()
  │     → for each vq: virtqueue_napi_schedule()
  └─ netif_tx_start_all_queues()

virtnet_close()
  ├─ netif_tx_disable()
  └─ virtnet_napi_disable()
```

**TX 路径（xmit → 完成）**：
```
ndo_start_xmit = start_xmit()
  ├─ skb_queue_tail(&sq->queue, skb)    → 把 skb 入 TX queue
  ├─ virtqueue_add_xmit/sg()            → 把 skb 拆成 sg list，提交到 send virtqueue
  └─ virtqueue_notify()                 → 通知 device

device 完成 TX（callback）:
  └─ virtnet_tx_notify()
        → napi_schedule(&sq->napi)
              → virtnet_poll()
                    ├─ virtqueue_detach_unused()
                    └─ napi_complete_done()
```

**RX 路径（device 收包 → poll → 协议栈）**：
```
device 收到包，触发 callback:
  └─ virtnet_rx_notify()
        → napi_schedule(&rq->napi)
              → virtnet_poll()
                    ├─ virtqueue_get_buf()
                    │     → 返回填充好的 skb
                    ├─ netif_receive_skb(skb)
                    └─ try_fill_recv()
                          → virtqueue_add()
                          → 补充 RX buffer（类似 stage04 refill）

try_fill_recv()
  → while (!virtqueue_full(rq->vq))
      skb = alloc_skb()
      virtqueue_add_inbuf()
  → virtqueue_kick(rq->vq)
```

### 3.3 关键数据结构对照

| stage04 | virtio-net | 说明 |
|---------|-----------|------|
| `struct tx_desc` | `struct send_queue + virtqueue` | TX 队列抽象 |
| `struct rx_desc` | `struct receive_queue + virtqueue` | RX 队列抽象 |
| `owner=CPU/DEV` | `avail->idx` vs `used->idx` | ownership 协议化 |
| `state=EMPTY/POSTED/DONE` | `desc[id].flags` | 槽位状态 |
| `rx_hw_pos` | `vq->last_avail_idx` | 消费位置追踪 |
| `ring_size` | `virtqueue_get_vring_size()` | 队列深度 |

### 3.4 virtio_net.h 关键定义

```c
/* virtio_net.h — 核心头文件 */

/* 设备特征位（决定行为）*/
#define VIRTIO_NET_F_CSUM      (1<<0)   // checksum offload
#define VIRTIO_NET_F_GUEST_CSUM (1<<5)  // guest 可以处理 partial csum
#define VIRTIO_NET_F_MAC       (1<<5)   // 设备有 MAC 地址
#define VIRTIO_NET_F_GSO       (1<<6)   // GSO support
#define VIRTIO_NET_F_HOST_TSO4 (1<<11)  // host TSO4
#define VIRTIO_NET_F_MRG_RXBUF (1<<15)  // 合并式 RX buffer

/* virtio net 头部（coalesced mode）*/
struct virtio_net_hdr {
    __u8 flags;           // VIRTIO_NET_HDR_F_* 标志
    __u8 gso_type;         // VIRTIO_NET_GSO_* 类型
    __virtio16 hdr_len;   // header 长度（含 Ethernet header）
    __virtio16 mss;       // Maximum Segment Size
    __virtio16 num_buffers; // 包合并时的 buffer 数量
};

/* 带 MAC 的头部（VIRTIO_NET_F_MAC 启用时）*/
struct virtio_net_hdr_mrg_rxbuf {
    struct virtio_net_hdr hdr;
    __virtio16 num_buffers;
    __u8 mac[6];          // MAC 地址
};
```

---

## 四、stage04 → virtio-net 映射详解

### 4.1 RX replenishment 对照

**stage04**：
```c
// poll 处理完一个 slot 后立刻 refill
stage04_refill_rx_slot(priv, idx);
// 同步在 poll 循环内完成
```

**virtio-net**：
```c
// try_fill_recv 在 poll 结束后调用
try_fill_recv(struct virtnet_info *vi, struct receive_queue *rq)
{
    int err;
    // 批量填充直到 virtqueue 满
    do {
        skb = alloc_skb();
        err = virtqueue_add_inbuf(rq->vq, skb, GFP_KERNEL);
    } while (err == 0 && !virtqueue_full(rq->vq));
    // 通知 host/device 可以继续写
    virtqueue_kick(rq->vq);
}
```

**核心差异**：stage04 同步在 poll 内 refill，virtio-net 用独立的 try_fill_recv 批量填充。

### 4.2 TX 路径对照

**stage04**：
```c
ndo_start_xmit()
  ├─ skb_linearize()           // 处理非线性 skb
  ├─ dma_map_single(DMA_TO_DEVICE)
  ├─ memcpy(skb->data → rx_buffer)  // 模拟 device DMA
  ├─ rxd->state = DONE
  └─ napi_schedule()
```

**virtio-net**：
```c
ndo_start_xmit = start_xmit()
  ├─ skb = skb_peek(&sq->queue)   // 取 pending skb
  ├─ xmit solemn = virtio_net_hdr_from_skb(skb)
  │       → 从 skb 提取 checksum/GSO 信息填入 virtio_net_hdr
  ├─ virtqueue_add_outbuf(sq->vq, sq->sg, num_buf, skb, GFP_ATOMIC)
  │       → 把 skb+sg list 提交到 send virtqueue
  └─ virtqueue_notify()
        → 发送门铃通知 device
```

### 4.3 NAPI poll 对照

**stage04**：
```c
stage04_poll(napi, budget)
  ├─ while (work_done < budget)
  │     ├─ 取 rx_poll_pos 的 desc
  │     ├─ 检查 DONE + CPU owner
  │     ├─ netif_receive_skb(skb)
  │     └─ refill slot
  └─ napi_complete_done()
```

**virtio-net**：
```c
virtnet_poll(napi, budget)
  ├─ receive_queue poll
  │     ├─ while (virtqueue_get_buf(rq->vq, &len))
  │     │     → 取出 device 写好的 skb
  │     ├─ netif_receive_skb(skb)
  │     └─ try_fill_recv()         ← refill 逻辑独立
  └─ send_queue poll
        ├─ virtqueue_get_buf(sq->vq, &len)
        └─ napi_complete_done()
```

### 4.4 DMA 操作对照

**stage04**（显式，每包映射）：
```c
dma_map_single(dev, skb->data, len, DMA_TO_DEVICE);   // TX
dma_map_single(dev, skb->data, buf_len, DMA_FROM_DEVICE); // RX refill
dma_unmap_single(dev, dma_addr, len, direction);
```

**virtio-net**（通过 virtqueue 间接，传输层隐藏细节）：
```c
// sg list 经 virtio 总线传输，DMA 由 virtio-pci/virtio-mmio 等 transport 处理
virtqueue_add_outbuf(vq, sg, num, skb, GFP_ATOMIC);
virtqueue_add_inbuf(vq, sg, num, skb, GFP_ATOMIC);
// 实际的 DMA 映射在 virtio_ring.c 的 virtqueue_add() 中
```

---

## 五、平台参数化详解

### 5.1 环境变量架构

```
stage05_virtio_param
├── env/stage05_virtio_param.env     ← 默认值（host）
├── output/resolved_host.env          ← host 平台解析结果
├── output/resolved_arm64_qemu-arm64.env ← ARM64+QEMU 解析结果
└── scripts/resolve_platform_env.sh  ← 平台解析脚本
```

### 5.2 关键解析规则

| 变量 | host 模式 | arm64+qemu 模式 |
|------|-----------|----------------|
| `TARGET_ARCH` | `host` | `arm64` |
| `RUN_MODE` | `host` | `qemu-arm64` |
| `HOST_CC` | `gcc` | `gcc` |
| `CROSS_COMPILE` | `(空)` | `aarch64-linux-gnu-` |
| `QEMU_BIN` | `(空)` | `qemu-system-aarch64` |
| `KERNEL_SOURCE_ROOT` | 需手动设置 | 需手动设置 |
| `KERNEL_BUILD_DIR` | 需手动设置 | 需手动设置 |
| `VIRTIO_NET_SOURCE` | 需手动设置 | 需手动设置 |

### 5.3 平台检查清单

在目标平台上执行以下检查：

```bash
# 基础工具链
which gcc; gcc --version | head -1
which aarch64-linux-gnu-gcc 2>/dev/null || echo "no cross compiler"

# QEMU
which qemu-system-aarch64 2>/dev/null || echo "no qemu-system-aarch64"

# 内核头文件/构建目录
ls /lib/modules/$(uname -r)/build 2>/dev/null && echo "KDIR OK" || echo "KDIR missing"

# virtio-net 源码
find /usr/src /lib/modules -name "virtio_net.c" 2>/dev/null | head -3

# debugfs（stage04 调试用）
mount | grep debugfs || sudo mount -t debugfs none /sys/kernel/debug
ls /sys/kernel/debug/netdev_stage04/ 2>/dev/null
```

---

## 六、smoke test 说明

stage05 本身不编译驱动，smoke 测试验证**环境就绪度**：

```bash
# 检查当前环境
make report
# 输出：host kernel / gcc / virtio_net.c 是否找到

# 检查源码可用性
make virtio-map
# 需要：VIRTIO_NET_SOURCE=/path/to/virtio_net.c

# 对照报告
make compare
# 输出：stage04 vs virtio-net 概念对照

# 平台矩阵
make platform-matrix
# 输出：host / x86_64+qemu / arm64+qemu 三个平台的能力矩阵

# 一键入口
make smoke   # 等价于 report + compare + platform-matrix
```

**当前环境问题**：`VIRTIO_NET_SOURCE` 未设置，导致 `virtio_net_map.md` 显示"virtio_net.c: not found"。

解决方法：
```bash
# 找到 virtio_net.c
find /usr/src /lib/modules -name "virtio_net.c" 2>/dev/null | head -3
# 或下载内核源码
export VIRTIO_NET_SOURCE=/path/to/linux/drivers/net/virtio_net.c
make virtio-map
```

---

## 七、踩坑记录

### 问题 1：`VIRTIO_NET_SOURCE` 未设置

```
virtio_net_map.md: virtio_net.c: not found
```

解决方法：设置环境变量或修改 `env/stage05_virtio_param.env`。

### 问题 2：arm64 交叉编译器缺失

```
aarch64-linux-gnu-gcc: command not found
```

安装方法：
```bash
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
```

### 问题 3：QEMU aarch64 缺失

```
qemu-system-aarch64: command not found
```

安装方法：
```bash
sudo apt install qemu-system-arm
```

---

## 八、与 stage04 的关键差异总结

| 维度 | stage04 | virtio-net |
|------|---------|-----------|
| 架构 | 单文件教学驱动 | 分层实现（netdev + virtqueue + transport） |
| TX buffer 管理 | 显式 tx_ring 数组 | virtqueue 抽象（avail/used ring 协议） |
| RX buffer 管理 | 显式 rx_ring + refill | virtqueue + 独立 try_fill_recv |
| DMA | 每包 dma_map/unmap_single | virtio transport 层处理（对 driver 透明） |
| ownership | owner 字段（CPU/DEV） | avail_idx vs used_idx（环形协议） |
| packet_type | dev_add_pack(0x88B7) | eth_type_trans + 内核协议栈自动分发 |
| NAPI | 单一 poll 函数 | 每个 virtqueue 一个 NAPI 实例 |
| device 模拟 | memcpy 模拟 DMA | 真实 virtio device（QEMU virtio-net） |
| 配置 | 模块参数 | virtio 特性位（feature bits）协商 |

---

## 九、扩展方向（stage06）

1. **ARM64 编译验证**：在 arm64+qemu 环境下编译 stage04 模块
2. **virtio-net 源码精读**：设置 VIRTIO_NET_SOURCE 后系统阅读 virtnet_probe/xmit/poll
3. **QEMU virtio-net device 模型**：理解前端（guest driver）后端（host tap）
4. **vring 协议深度**：avail/used ring 的 descriptor table、多 buffer 场景
5. **性能对比**：stage04（memcpy 模拟）vs virtio-net（真实 DMA）的性能差异
