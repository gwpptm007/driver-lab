# 05_DEEP_LEARNING — XDP 深度解析与调用链

## 1. XDP 在数据包处理流程中的位置

```
Packet arrives at NIC (DMA)
        ↓
[XDP] ←─────────────── 最早处理点（驱动层，在 build_skb 之前）
        ↓ XDP_PASS
[GRO] ←─────────────── stage13 已实现
        ↓
[netif_receive_skb] ←── stage13 已实现
        ↓
[protocol stack]
```

**关键区别**：
- XDP：无法访问 `sk_buff`，只能处理 `xdp_buff`
- TC：可以访问完整的 `sk_buff`，但有更大开销
- XDP + TC：XDP 先处理，TC 作为二次过滤

---

## 编译 BPF 程序 — 依赖 clang + llvm

XDP program 是 ELF 目标文件（`.o`），需要 clang 编译为 BPF 目标架构：

```bash
# 安装工具链
sudo apt install clang llvm

# 编译示例（从 .c 到 .o）
clang -O2 -target bpf -Wall \
  -I /usr/include/bpf \
  -c xdp_pass_kern.c -o xdp_pass_kern.o

# 验证 section 信息
llvm-objdump -h xdp_pass_kern.o
```

stage14 已提供 `bpf/build_xdp.sh` 脚本，一键编译所有示例。预编译好的 `.o` 文件可以离线复制到无 clang 的测试机。

---

## 2. 完整调用链 — XDP program 加载

```
用户空间                          内核
─────────────────────────────────────────────────────────────
ip link set dev eth0 xdp obj xdp.o
        │
        │  netlink socket
        ▼
netlink_rcv(skb)
        │
        │  XDP_SETUP_PROG netlink msg
        ▼
rtnetlink_rcv_msg()
        │
        ▼
ndo_bpf(dev, bpf={.command=XDP_SETUP_PROG, .prog=prog})
        │  ←────────────── 驱动实现的 ndo_bpf 回调
        ▼
struct netdev_bpf {
    .command = XDP_SETUP_PROG,
    .prog    = bpf_prog对象,
};
```

### 内核内部链路（net/core/dev.c）

```
ndo_bpf (函数指针)
        │
        ├──→ rtnl_lock() 保护
        │
        ├──→ bpf_prog_inc(prog)     // 增加引用计数
        │
        ├──→ rcu_assign_pointer(dev->xdp_prog, prog)
        │         ↓
        │         设置 struct net_device::xdp_prog (RCU 保护)
        │
        └──→ netdev_info(dev, "XDP program loaded: %s\n", prog->aux->name)
```

### 驱动侧 `stage14_xdp()` 实现

```c
static int stage14_xdp(struct net_device *ndev, struct netdev_bpf *bpf)
{
    struct stage14_priv *priv = netdev_priv(ndev);

    switch (bpf->command) {
    case XDP_SETUP_PROG: {
        struct bpf_prog *prog = bpf->prog;
        if (prog) {
            // 增加引用计数（防止卸载时被释放）
            bpf_prog_inc(prog);
            // RCU 保护写入（其他 CPU 的 XDP 处理路径可安全读取）
            rcu_assign_pointer(priv->xdp_prog, prog);
            // 统计
            atomic64_inc(&priv->xdp_prog_set_count);
            netdev_info(ndev, "XDP program loaded: %s\n", prog->aux->name);
        } else {
            // 卸载：RCU 替换 + 同步 + 释放
            struct bpf_prog *old = rcu_dereference(priv->xdp_prog);
            rcu_assign_pointer(priv->xdp_prog, NULL);
            if (old) {
                synchronize_net();        // 等待所有 RCU 读者完成
                bpf_prog_put(old);        // 释放 prog
                atomic64_inc(&priv->xdp_prog_clear_count);
            }
            netdev_info(ndev, "XDP program unloaded\n");
        }
        return 0;
    }
    default:
        return -EINVAL;
    }
}
```

---

## 3. 完整调用链 — RX 路径 + XDP 处理

### 整体数据流

```
backend_workfn()          NAPI poll()              XDP 处理
───────────────────      ──────────────           ──────────
填充 RX slot
标记 S14_SLOT_READY
触发 IRQ
        │
        ▼
stage14_raise_irq()
        │
        │  queue_work(irq_wq, irq_work)
        ▼
stage14_irq_workfn()
        │
        │  napi_schedule_prep() + __napi_schedule()
        ▼
stage14_napi_poll(budget=64)
        │
        │  while (rx_ready && work < budget)
        ▼
stage14_consume_rx_one()
        │
        ├──→ rcu_dereference(priv->xdp_prog)  ←─── 检查是否有 XDP program
        │         │
        │         ▼ (如果有 XDP program)
        │   stage14_xdp_process()
        │         │
        │         ├──→ xdp_init_buff()
        │         ├──→ xdp_prepare_buff()
        │         ├──→ bpf_prog_run_xdp(prog, &xdp)
        │         │         │
        │         │         ▼
        │         │   BPF program 执行
        │         │         │
        │         ├──→ switch(act) {
        │         │     case XDP_PASS: 继续 build_skb 路径
        │         │     case XDP_DROP: page_pool_put_page() → return 0
        │         │     case XDP_TX:   (软模型只统计)
        │         │     case XDP_REDIRECT: xdp_do_redirect() (软模型只统计)
        │         │   }
        │         ▼
        │   if (XDP_DROP) {
        │       page_pool_put_page(q->pp, page);  ← 归还 page，不上送
        │       refill_rx_slot();
        │       return 0;
        │   }
        │
        │ (XDP_PASS: 继续 build_skb 路径)
        ▼
build_skb(buf, rx_buf_size)
        │
        ├──→ skb = alloc_skb()
        ├──→ skb_put(skb, len)
        ├──→ skb->dev = ndev
        ├──→ skb->protocol = eth_type_trans()
        │
        ▼
if (GRO enabled)
    napi_gro_receive(&q->napi, skb)  ←─── GRO 批量合并
else
    netif_receive_skb(skb)             ←─── 逐包上送
        │
        ▼
protocol stack (IP → TCP/UDP)
```

### `stage14_xdp_process()` 详细实现

```c
static int stage14_xdp_process(struct stage14_queue *q,
                                struct stage14_buf_slot *s,
                                void *buf, u32 len)
{
    struct stage14_priv *priv = q->priv;
    struct bpf_prog *prog;
    struct xdp_buff xdp;
    u32 act;

    // RCU 保护读取（可与其他 CPU 的 ndo_bpf 并发）
    prog = rcu_dereference(priv->xdp_prog);
    if (!prog)
        return XDP_PASS;

    // 初始化 XDP buffer（现代 API）
    xdp_init_buff(&xdp, priv->rx_buf_size, &q->xdp_rxq);
    xdp_prepare_buff(&xdp, buf, 0, len, false);

    // 运行 BPF program
    act = bpf_prog_run_xdp(prog, &xdp);

    switch (act) {
    case XDP_PASS:
        atomic64_inc(&q->stats.xdp.xdp_pass);
        return XDP_PASS;  // 继续走 build_skb
    case XDP_DROP:
        atomic64_inc(&q->stats.xdp.xdp_drop);
        return XDP_DROP;  // 丢弃，不上送协议栈
    case XDP_TX:
        atomic64_inc(&q->stats.xdp.xdp_tx);
        return XDP_TX;    // 软模型只统计
    case XDP_REDIRECT:
        atomic64_inc(&q->stats.xdp.xdp_redirect);
        return XDP_REDIRECT;  // 软模型只统计
    default:
        atomic64_inc(&q->stats.xdp.xdp_err);
        return XDP_PASS;
    }
}
```

---

## 4. xdp_buff vs sk_buff 对比

| 字段 | xdp_buff | sk_buff | 说明 |
|------|-----------|----------|------|
| `data` | ✅ | ✅ | 数据起始指针 |
| `data_end` | ✅ | ✅ | 数据结束指针 |
| `data_meta` | ✅ | ❌ | XDP 元数据区 |
| `data_hard_start` | ✅ | ❌ | XDP 预留空间（headroom） |
| `dev` | ❌ | ✅ | 通过 rxq->dev 间接访问 |
| `protocol` | ❌ | ✅ | 需要从 ethhdr 解析 |
| `hash` | ❌ | ✅ | 可计算 |
| `vlan*` | ❌ | ✅ | VLAN tag 处理 |

---

## 5. xdp_rxq_info 与内存模型

### xdp_rxq_info 结构

```c
struct xdp_rxq_info {
    struct net_device *dev;     // 所属 netdev
    u32 queue_index;            // RX queue 编号
    u32 reg_state;              // 注册状态标志
    struct xdp_mem_info mem;    // 内存类型
    unsigned int napi_id;        // 关联的 NAPI ID
};
```

### 内存模型类型

```c
enum xdp_mem_type {
    MEM_TYPE_PAGE_SHARED = 0,   // 分页共享引用模型
    MEM_TYPE_PAGE_ORDER0,        // 全页模型（order-0 page）
    MEM_TYPE_PAGE_POOL,          // page_pool 分配的页
    MEM_TYPE_XSK_BUFF_POOL,     // AF_XDP 专用
};
```

### 驱动中的注册时机

```c
// page_pool 创建之后，NAPI 注册之后
q->pp = stage14_create_page_pool(q);      // 1. 创建 page_pool
/* ... */
netif_napi_add(ndev, &q->napi, ...);       // 2. 注册 NAPI（获取 napi_id）
/* ... */
xdp_rxq_info_reg(&q->xdp_rxq, ndev, q->qid, q->napi.napi_id);  // 3. 注册 RXQ
xdp_rxq_info_reg_mem_model(&q->xdp_rxq, MEM_TYPE_PAGE_ORDER0, NULL); // 4. 注册内存模型
```

---

## 6. ndo_bpf 命令详解

| 命令 | 方向 | 说明 |
|------|------|------|
| `XDP_SETUP_PROG` | 加载 | `bpf->prog` 指向 BPF program |
| `XDP_SETUP_PROG_HW` | 硬件卸载 | 硬件级 XDP 支持（mlx5 等） |
| `XDP_QUERY_PROG` | 查询 | 查询已加载 program ID（5.15 不可用） |

---

## 7. XDP_REDIRECT 内部机制（真实驱动）

```c
// net/core/dev.c
int xdp_do_redirect(struct net_device *dev, struct xdp_buff *xdp,
                    struct bpf_prog *xdp_prog)
{
    struct redirect_info *ri = dev_get_redirect_info(dev);
    struct bpf_map *map = READ_ONCE(ri->map);
    u32 index = ri->ifindex;

    if (map) {
        // devmap: 重定向到其他网络设备
        dev = devmap_lookup_dev(ri, index);
        if (!dev)
            return -EINVAL;
    }

    // 发送到目标设备的 TX 队列
    return dev->netdev_ops->ndo_xdp_xmit(dev, xdp);
}
```

---

## 8. BPF Map 深度解析

### per-CPU 计数 map（statsmap）

```c
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 256);
    __type(key, u32);
    __type(value, u64);
} xdp_stats SEC(".maps");

SEC("xdp")
int xdp_count(struct xdp_buff *xdp)
{
    u32 key = 0;
    u64 *cnt;

    cnt = bpf_map_lookup_elem(&xdp_stats, &key);
    if (cnt)
        *cnt += 1;

    return XDP_PASS;
}
```

### devmap（设备重定向）

```c
struct {
    __uint(type, BPF_MAP_TYPE_DEVMAP);
    __uint(max_entries, 64);
    __type(key, u32);
    __type(value, struct bpf_devmap_val);
} tx_port SEC(".maps");

SEC("xdp")
int xdp_redirect(struct xdp_buff *xdp)
{
    return bpf_redirect_map(&tx_port, 0, 0);  // 重定向到 tx_port[0]
}
```

### cpumap（CPU 负载均衡）

```c
struct {
    __uint(type, BPF_MAP_TYPE_CPUMAP);
    __uint(max_entries, 64);
    __type(key, u32);
    __type(value, struct bpf_cpumap_val);
} cpu_map SEC(".maps");

SEC("xdp")
int xdp_load_balance(struct xdp_buff *xdp)
{
    return bpf_redirect_map(&cpu_map, 0, 0);  // 重定向到 CPU 队列
}
```

---

## 9. AF_XDP vs XDP 深度对比

| 维度 | XDP | AF_XDP |
|------|-----|--------|
| 处理位置 | 内核（驱动层，最早） | 用户空间（通过 socket） |
| 性能 | 最高（零拷贝） | 接近 XDP（零拷贝模式） |
| 灵活性 | BPF program（受限） | 普通 C 程序（完全控制） |
| 内存模型 | DMA direct | UMEM（共享内存） |
| 用途 | DDoS防护、转发、负载均衡 | 用户空间网络处理、压测 |
| 硬件要求 | 需要驱动支持 | 需要驱动支持 + UMEM |

---

## 10. Cilium 中的 XDP 实践

Cilium 利用 XDP 实现高性能网络：

```c
// Cilium HTTP 策略 enforcement（简化版）
SEC("xdp")
int cilium_policy(struct xdp_buff *xdp)
{
    void *data = xdp->data;
    void *data_end = xdp->data_end;

    struct ethhdr *eth = data;
    if (eth + 1 > data_end)
        return XDP_PASS;

    if (eth->h_proto == htons(ETH_P_IP)) {
        struct iphdr *ip = data + sizeof(*eth);
        if (ip + 1 > data_end)
            return XDP_PASS;

        /* 查询 HTTP 允许列表 */
        if (ip->protocol == IPPROTO_TCP) {
            if (is_blocked_ip(ip->saddr))
                return XDP_DROP;
        }
    }

    return XDP_PASS;
}
```

---

## 11. XDP 与 TC (Traffic Control) 的关系

```
Packet arrives
        ↓
[XDP] ←─────────────── 最早处理点（驱动层）
        ↓ XDP_PASS
[TC ingress] ←─── qdisc attach point
        ↓
[skb → protocol stack]
```

**关键区别**：
- XDP：无法访问 `sk_buff`，只能处理 `xdp_buff`
- TC：可以访问完整的 `sk_buff`，但有更大开销
- XDP + TC：XDP 先处理，TC 作为二次过滤

---

## 12. stage14 与 stage15 的分界线

```
stage14: XDP 基础
  - ndo_bpf 回调
  - xdp_buff 处理
  - XDP_PASS/DROP 统计
  - 软模型限制（无真正 TX/REDIRECT）

stage15 候选方向：
  - XDP_TX 真实实现（需要 virtio-net 或 soft MAC）
  - XDP_REDIRECT 到 veth pair（用户空间）
  - XDP + page_pool 深度整合
  - TC + XDP 联合编程
```

---

## 13. 学习路径

```
stage14 (XDP 基础)
        ↓
理解 xdp_buff、xdp_action
掌握 ndo_bpf 回调
理解 XDP 与 GRO 的关系
        ↓
进阶路径
├── 方向A: 云原生网络
│   ├── Cilium XDP 源码分析
│   ├── XDP + eBPF 组合
│   └── Kata Containers / gVisor
│
├── 方向B: DPDK / AF_XDP
│   ├── AF_XDP zero-copy 机制
│   ├── DPDK pipeline 设计
│   └── 性能对比分析
│
└── 方向C: 高速转发
    ├── XDP_TX 真实实现
    ├── XDP_REDIRECT 到物理网卡
    └── TC + XDP 联合编程
```
