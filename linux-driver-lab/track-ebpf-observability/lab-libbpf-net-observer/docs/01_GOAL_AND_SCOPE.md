# 01_GOAL_AND_SCOPE — 原理篇

## 1. 本 Lab 的目标

从 Phase 3 的 **bpftrace 脚本** 升级到 **C/libbpf 编译型观测工具**，掌握 BPF CO-RE（Compile Once, Run Everywhere）开发范式。

核心问题：

```text
1. 如何将 bpftrace 一行脚本翻译为 C + libbpf 程序？
2. vmlinux.h 是什么？BTF 如何支撑 CO-RE？
3. BPF skeleton vs 原生 libbpf API 有什么区别？
4. tracepoint context 的内存布局如何手动匹配？
5. ringbuf（共享内存环形队列）相比 perf buffer 的优势是什么？
6. libbpf 0.5（Ubuntu 22.04）与 clang 14 有哪些兼容性陷阱？
```

## 2. 技术架构

### 2.1 整体数据流

```text
┌─────────────────────────────────────────────────────────────────────┐
│                          Linux Kernel                                │
│                                                                      │
│  Tracepoints (ABI stable)                                           │
│  ┌──────────────────────┐    ┌──────────────────────┐               │
│  │ net:netif_receive_skb│    │ net:napi_gro_receive │               │
│  │ net:net_dev_queue    │    │ net:net_dev_start_xmit│               │
│  │ skb:kfree_skb        │    │                      │               │
│  └──────┬───────────────┘    └──────┬───────────────┘               │
│         │                           │                                │
│         ▼                           ▼                                │
│  ┌──────────────────────────────────────────────────────┐           │
│  │              BPF Programs (skb_observer.bpf.c)        │           │
│  │                                                       │           │
│  │  tp_netif_receive_skb() → submit_event(EVENT_RX)     │           │
│  │  tp_napi_gro_receive()  → submit_event(EVENT_GRO)    │           │
│  │  tp_net_dev_queue()     → submit_event(EVENT_TX_QUEUE)│          │
│  │  tp_net_dev_start_xmit()→ submit_event(EVENT_TX_XMIT) │          │
│  │  tp_kfree_skb()         → submit_event(EVENT_DROP)   │           │
│  │                                                       │           │
│  │  Maps:                                                │           │
│  │  ┌──────────────┐  ┌──────────────────┐              │           │
│  │  │ events (RB)  │  │ event_counts     │              │           │
│  │  │ RINGBUF 2MB  │  │ PERCPU_ARRAY[5]  │              │           │
│  │  └──────┬───────┘  └──────────────────┘              │           │
│  └─────────┼────────────────────────────────────────────┘           │
│            │                                                         │
└────────────┼─────────────────────────────────────────────────────────┘
             │ ringbuf (mmap'd shared memory)
             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   Userspace Loader (skb_observer.c)                   │
│                                                                       │
│  bpf_object__open()  →  解析 ELF BPF .o                              │
│  bpf_object__load()  →  加载到内核 (verifier + JIT)                  │
│  bpf_program__attach() → attach tracepoint (perf_event_open)        │
│  ring_buffer__new()   →  mmap events map                             │
│  ring_buffer__poll()  →  消费事件 (100ms interval)                   │
│  handle_event()       →  格式化输出 + 统计                           │
│                                                                       │
│  输出: 实时事件流 + 分类统计 (per-event counts)                      │
└───────────────────────────────────────────────────────────────────────┘
```

### 2.2 BPF CO-RE 编译流程

```text
┌────────────────────┐
│  /sys/kernel/btf/  │  内核 BTF 信息 (类型、结构体布局)
│  vmlinux           │
└────────┬───────────┘
         │ bpftool btf dump
         ▼
┌────────────────────┐
│  vmlinux.h         │  完整内核类型头文件 (自动生成，~1MB+)
│                    │  包含所有 struct, enum, typedef
└────────┬───────────┘
         │ #include + clang -target bpf
         ▼
┌────────────────────┐
│  skb_observer.bpf.o│  BPF ELF 目标文件
│                    │  Sections: .BTF, .BTF.ext, tracepoint/...
│                    │  Maps: events (ringbuf), event_counts (percpu array)
└────────┬───────────┘
         │ bpf_object__open() + bpf_object__load()
         ▼
┌────────────────────┐
│  BPF verifier      │  内核验证: 无循环、内存安全、类型匹配
│  JIT compiler      │  翻译为 x86/arm64 原生指令
└────────────────────┘
```

### 2.3 核心概念对比：bpftrace vs C/libbpf

| 维度 | bpftrace | C/libbpf |
|------|----------|----------|
| **语言** | awk-like DSL | C (clang -target bpf) |
| **加载方式** | bpftrace 解释执行 | 编译为 ELF .o, 由 userspace loader 加载 |
| **类型系统** | 动态类型 (count, sum, hist) | 强类型 (C struct, enum) |
| **数据结构** | bpftrace 内置 map | 手动定义 `SEC(".maps")` |
| **事件输出** | `printf()`, `print()` | ringbuf → userspace poll |
| **调试** | `-d` 调试模式 | 编译期检查 + BPF verifier 报错 |
| **可移植性** | 需要目标机器安装 bpftrace | 编译产物 .o 可独立部署 |
| **性能** | 解释执行，略有开销 | JIT 编译，接近原生 |
| **灵活性** | 适合快速原型 | 适合生产级工具 |

## 3. 关键技术细节

### 3.1 tracepoint context 手工布局

BPF tracepoint 程序的 context 不是 `vmlinux.h` 中的结构体，而是 raw buffer。每个 tracepoint 的字段布局在 `/sys/kernel/debug/tracing/events/<subsys>/<name>/format` 中定义。

**示例: netif_receive_skb (内核 6.8)**

```text
field:void * skbaddr;          offset:8;  size:8
field:unsigned int len;        offset:16; size:4
field:__data_loc char[] name;  offset:20; size:4
```

对应的 C struct:

```c
struct tp_netif_receive_skb {
    __u64 __trace_entry;    /* common fields (type+flags+preempt+pid) offset:0 */
    __u64 skbaddr;          /* offset:8 */
    __u32 len;              /* offset:16 */
    __u32 name;             /* __data_loc char[]  offset:20 */
};
```

**关键规则**:
1. 前 8 字节永远是 `common_type(2) + common_flags(1) + common_preempt_count(1) + common_pid(4)`
2. 后续字段的 offset 必须精确匹配 format 文件
3. `__data_loc` 字段是 u32，编码为 `(length << 16) | offset`，offset 相对于 context 起始（而非字段本身）

### 3.2 `__data_loc` 字符串解码

```c
static __always_inline void read_tp_name(const void *ctx_base,
                                          const void *name_field,
                                          char *out, int out_len)
{
    __u32 data_loc;
    bpf_probe_read_kernel(&data_loc, sizeof(data_loc), name_field);

    __u16 offset = data_loc & 0xFFFF;   // 字符串相对 ctx 起始的偏移
    __u16 length = data_loc >> 16;       // 字符串长度

    if (offset > 0 && length > 0)
        bpf_probe_read_kernel_str(out, out_len,
                                  (const char *)ctx_base + offset);
}
```

**常见错误**: 将 offset 加到字段指针上 `field_ptr + offset`，实际应该用 `ctx_base + offset`。

### 3.3 ringbuf 事件提交

```c
static __always_inline int submit_event(__u32 type, __u32 len,
                                         const char *ifname)
{
    struct skb_event *ev;
    ev = bpf_ringbuf_reserve(&events, sizeof(*ev), 0);
    if (!ev) return 0;  // ringbuf 满，丢弃

    ev->type = type;
    ev->cpu = bpf_get_smp_processor_id();
    ev->len = len;
    ev->timestamp = bpf_ktime_get_ns();
    __builtin_memcpy(ev->ifname, ifname, sizeof(ev->ifname));
    bpf_ringbuf_submit(ev, 0);

    // 更新 per-event 计数 (~OBSERVABILITY 开销)
    __u32 key = type;
    __u64 *val = bpf_map_lookup_elem(&event_counts, &key);
    if (val) __sync_fetch_and_add(val, 1);

    return 0;
}
```

### 3.4 原生 libbpf API vs Skeleton

| 特性 | Skeleton (bpftool gen) | 原生 API |
|------|----------------------|----------|
| **依赖** | 需要 bpftool 生成 .skel.h | 仅需 libbpf |
| **代码量** | 少（自动生成 open/load/attach） | 多（手动调用 API） |
| **兼容性** | libbpf >= 0.8 推荐 | 兼容 libbpf 0.5+ |
| **.rodata.str 问题** | libbpf 0.5 会为 rodata 创建 map，失败 | 原生 API 忽略未知 section |
| **适用场景** | 新系统 (Ubuntu 24.04+) | 旧系统 (Ubuntu 22.04) |

我们的选择: **原生 libbpf API**，因为目标机器是 Ubuntu 22.04 + libbpf 0.5。

```c
// 原生 API 四步流程
struct bpf_object *obj = bpf_object__open(bpffile);   // 1. 解析 ELF
bpf_object__load(obj);                                 // 2. 加载到内核

bpf_object__for_each_program(prog, obj) {              // 3. attach
    struct bpf_link *link = bpf_program__attach(prog);
}

struct ring_buffer *rb = ring_buffer__new(             // 4. ringbuf
    bpf_map__fd(events_map), handle_event, NULL, NULL);
```

### 3.5 Per-CPU 数组统计读取

```c
// BPF 内核端: per-CPU 原子递增
__u32 key = type;
__u64 *val = bpf_map_lookup_elem(&event_counts, &key);  // 返回当前 CPU 的槽
if (val) __sync_fetch_and_add(val, 1);

// Userspace: 跨所有 CPU 求和
int ncpus = libbpf_num_possible_cpus();
__u64 *per_cpu = malloc(ncpus * sizeof(__u64));
for (key = 0; key < EVENT_MAX; key++) {
    __u64 sum = 0;
    bpf_map_lookup_elem(fd, &key, per_cpu);  // 读取 key 对应的 per-CPU 数组
    for (int i = 0; i < ncpus; i++)
        sum += per_cpu[i];
}
```

## 4. 兼容性问题与解决方案

### 4.1 libbpf 0.5 + clang 14: .rodata.str1.1

**现象**: skeleton 模式报 `failed to find skeleton map '.rodata.str1.1'`

**原因**: clang 14 为 BPF 程序中的字符串常量生成 `.rodata.str1.1` section。libbpf 0.5 的 skeleton 机制会为每个 section 创建 map，但不认识 rodata.str1.1 section 导致失败。

**解决方案**:
1. BPF 代码中消除所有字符串字面量（包括 `"<?>"` 等）
2. 使用原生 libbpf API（`bpf_object__open`）替代 skeleton
3. Makefile 中添加 `-fno-jump-tables -fno-stack-protector` 减少额外 section
4. 备用方案: `llvm-objcopy --remove-section=.rodata.str1.1`

### 4.2 BTF 缺失: -g0 禁止调试信息

**现象**: `libbpf: BTF is required, but is missing or corrupted.`

**原因**: `clang -g0` 不仅移除 DWARF 调试信息，也移除了 BPF 必需的 `.BTF` section。

**解决**: 使用 `-g` (默认) 保留 BTF，rodata.str 问题通过方案 4.1 解决。

### 4.3 tracepoint context 布局变化

**现象**: `len=4294938239` (垃圾值)、`ifname=<?>`

**原因**: 不同内核版本的 tracepoint format 不同。例如 `netif_receive_skb` 在 offset 8 是 `skbaddr`（不是 `name`），`name` 在 offset 20。

**解决**: 从目标机器 `/sys/kernel/debug/tracing/events/.../format` 读取实际布局，手工定义 struct，用显式 padding 字段保证对齐。

## 5. 观测数据解读

### 5.1 事件流模式

正常 ping 流量（ICMP echo request/reply）对应的事件模式：

```text
TX 方向 (ping 发出):
  TX-QUEUE (len=98)  →  TX-XMIT (len=98)    ← ICMP request (84 data + 14 L2 header)

RX 方向 (ping 收到回复):
  RX (len=84)  →  GRO (len=0)                ← ICMP reply (70 data + 14 L2 header)
                    GRO 不传 len (始终为 0)
```

### 5.2 预期统计特征

| 特征 | 含义 |
|------|------|
| RX ≈ GRO | NAPI 模式下每个 RX 包都经过 GRO（即使单包不合并） |
| TX-QUEUE = TX-XMIT | 每个排队包最终都通过驱动发送 |
| DROP = 0 (正常流量) | 无丢包路径触发 |
| CPU 分布 | RX/ GRO 集中在网卡 IRQ CPU，TX 分布在发起 ping 的 CPU |

## 6. 与 Phase 1-3 的关系

```text
Phase 1: bpftrace + kprobe     — 单点观测 (netif_rx)
Phase 2: bpftrace + kprobe     — 路径观测 (NAPI/softirq/netif_rx)
Phase 3: bpftrace + tracepoint — ABI 稳定观测 (5 tracepoints)
Phase 4: C/libbpf + tracepoint — 编译型观测 (本 Lab)
         ↑ 仅提供工具不同，观测对象与 Phase 3 完全一致
```

**Phase 4 的核心价值**: 证明 bpftrace 原型可通过 C/libbpf 转化为生产级工具，同时理解 libbpf 工程化细节（BTF、CO-RE、ringbuf、per-CPU map）。
