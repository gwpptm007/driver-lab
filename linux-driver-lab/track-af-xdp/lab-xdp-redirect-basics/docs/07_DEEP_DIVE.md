# 07_DEEP_DIVE - 深度原理

## 本 lab 解决什么问题

在正式写 AF_XDP socket 之前，先把 XDP 基础跑通。需要掌握：

```
clang 编译 BPF
    ↓
libbpf 加载 BPF object
    ↓
XDP attach 到网卡
    ↓
XDP action 控制（PASS / DROP / REDIRECT）
    ↓
stats map 统计
    ↓
理解 AF_XDP socket 如何通过 XSKMAP 接入
```

## 核心原理

### 1. XDP 是什么

XDP（Express Data Path）是 Linux 内核提供的高性能数据包处理框架：

```
网卡收到包
    ↓
送到 DMA 缓冲区（提前分配好的）
    ↓
XDP hook（在驱动层，还没有分配 skb）
    ↓
执行 XDP program（返回 action）
    ↓
        PASS  → 交给内核网络栈（分配 skb）
        DROP  → 丢弃（不占 skb，直接还 DMA 缓冲区）
        REDIRECT → 转发到其他网卡或 socket
```

**关键**：XDP 在 `skb` 分配之前就处理包，所以比普通内核路径快。

### 2. BPF 程序结构

本 lab 的 BPF 程序 `xdp_redirect_basics`：

```
struct config_map  — 用户态写入 action（PASS/DROP/REDIRECT）
struct stats_map  — per-CPU 统计（packets/bytes）
struct xsks_map   — XSKMAP（下一站 AF_XDP socket fd 写入这里）

SEC("xdp")
int xdp_redirect_basics(struct xdp_md *ctx)
    ctx->data     → 数据包起始指针（Ethernet 头）
    ctx->data_end → 数据包结束指针
    ctx->rx_queue_index → 收到包的队列 ID
```

**注意**：BPF 程序不能调用任意函数，只能调用 `bpf_*` helper 函数。

### 3. XDP Action

| Action | 含义 | 性能 |
|--------|------|------|
| XDP_PASS | 放行到内核协议栈 | 中等（还是要走内核） |
| XDP_DROP | 直接丢弃 | 最高（不分配 skb） |
| XDP_REDIRECT | 重定向到其他网卡或 socket | 高（绕过部分内核） |
| XDP_TX | 从同一网卡发回去 | 高 |

### 4. 为什么需要 generic (SKB) mode

XDP 有三种模式：

| 模式 | 说明 | 兼容性 |
|------|------|--------|
| SKB (generic) | 最通用，内核软件模拟 | 所有网卡 |
| DRV (native) | 驱动原生支持 | 需要驱动支持 |
| HW (offload) | 硬件 offload | 需要网卡支持 |

vmxnet3 只支持 SKB mode，所以本 lab 用 `--mode skb`。

### 5. libbpf 加载流程

```
bpf_object__open_file()      打开 .bpf.o 文件，解析 ELF
        ↓
bpf_object__load()           加载 maps 和 program 到内核
        ↓
bpf_object__find_program_by_name() 找到 "xdp_redirect_basics"
        ↓
bpf_program__fd()            获取 program fd
        ↓
bpf_xdp_attach()             把 program fd attach 到网卡 ifindex
```

### 6. map 的 per-CPU 统计

```c
struct stats_map = BPF_MAP_TYPE_PERCPU_ARRAY
    每个 CPU 有独立的值
    统计时需要把所有 CPU 的值加起来
```

为什么用 per-CPU？因为多核下不加锁也能原子累加，避免竞争。

### 7. XSKMAP 与 AF_XDP 的关系

```
AF_XDP socket 创建后：
    socket fd → XSKMAP[queue_id]

XDP 程序执行时：
    bpf_redirect_map(&xsks_map, ctx->rx_queue_index, XDP_PASS)
           ↓
    包被送到对应 AF_XDP socket 的 RX ring
           ↓
    用户态程序通过 umem 收包（零拷贝）
```

本 lab 只有 xsks_map，还没有 AF_XDP socket，所以 REDIRECT 只是 dry-run。

## 调用链

### 编译（用户态）

```
xdp_redirect_basics.bpf.c
    clang -target bpf
        ↓
    xdp_redirect_basics.bpf.o (ELF 格式)
```

### 加载（用户态 libbpf）

```
xdp_loader.c main()
    bpf_object__open_file("xdp_redirect_basics.bpf.o")
        ↓
    bpf_object__load(obj)
        ↓ 遍历 ELF，分配 map，创建 program
    bpf_object__find_program_by_name(obj, "xdp_redirect_basics")
        ↓
    bpf_xdp_attach(ifindex, prog_fd, xdp_flags, NULL)
        ↓ 写入 netlink socket，驱动注册 XDP hook
```

### 包处理（内核态）

```
网卡 DMA 收包
    ↓
XDP hook 调用 xdp_redirect_basics()
    ↓
查 config_map[0] → action
    ↓
action == XDP_DROP?
        ↓ yes → return XDP_DROP（还 DMA 缓冲区）
        ↓ no  → count_action(XDP_PASS) → return XDP_PASS
    ↓
用户态 poll stats_map，汇总 per-CPU 计数
```

## 关键数据结构

### struct xdp_md

```c
struct xdp_md {
    __u32 data;
    __u32 data_end;
    __u32 ingress_ifindex;   // 收到包的网卡
    __u32 rx_queue_index;    // 队列号，用于 XSKMAP lookup
};
```

### struct xdp_action_stat

```c
struct xdp_action_stat {
    __u64 packets;
    __u64 bytes;
};
```

### 三张 map 的区别

| Map | 类型 | 用途 | 写入方 |
|-----|------|------|--------|
| config_map | ARRAY | 存 action（0=PASS, 1=DROP, 2=REDIRECT） | 用户态 |
| stats_map | PERCPU_ARRAY | 统计每个 action 的包数/字节数 | BPF 程序 |
| xsks_map | XSKMAP | 存 AF_XDP socket fd | 用户态（下一站） |

## 理解要点

1. **XDP 在 skb 之前**：比 iptables/nftables 更快，因为不分配 skb
2. **BPF 是受限的 C**：不能调用任意函数，只能用 bpf_* helpers，不能随意指针运算
3. **per-CPU map 无锁**：多核统计不需要锁，因为每核独立
4. **XSKMAP 是 AF_XDP 的入口**：socket fd 写入 map，XDP program 通过 queue_id 查找
5. **generic mode 有 overhead**：SKB mode 需要分配 skb，所以比 native mode 慢，但兼容性好

## 后续延伸

- 本 lab：`bpf_redirect_map(&xsks_map, queue, XDP_PASS)` 还不指向真实 socket
- 下一站：创建 AF_XDP socket，bind 到 umem，把 fd 写入 xsks_map，实现真正的零拷贝收包
