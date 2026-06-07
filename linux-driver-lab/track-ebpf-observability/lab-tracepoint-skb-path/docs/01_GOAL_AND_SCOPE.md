# 01_GOAL_AND_SCOPE — 原理篇

## 1. 本 Lab 的目标

从 Phase 2 的 **kprobe 函数级观测** 升级到 **tracepoint ABI 级观测**，掌握 Linux 网络栈中 skb（socket buffer）在 RX/TX/drop 路径上的可观测性。

核心问题：

```text
1. skb 在 RX/TX 路径上经过哪些稳定 tracepoint？
2. tracepoint 相比 kprobe 为什么更稳定？
3. 如何用 tracepoint 观察 skb 的协议、设备、长度、方向？
4. 如何把 Phase 2 的 NAPI/softirq 证据继续串到 skb 层路径？
```

## 2. 前置知识

### 2.1 Linux 网络数据包路径概览

```text
                    ┌──────────────────────────────────────┐
                    │            Userspace                  │
                    │       (ping / iperf3 / nc)            │
                    └──────┬───────────────────┬───────────┘
                           │ sendto()          │ recvfrom()
                           ▼                   ▲
                    ┌──────────────┐    ┌──────────────┐
                    │  Socket Layer │    │  Socket Layer │
                    └──────┬───────┘    └──────▲───────┘
                           │                   │
                           ▼                   │
                    ┌──────────────┐    ┌──────────────┐
                    │  TCP/UDP     │    │  TCP/UDP     │
                    │  (L4)        │    │  (L4)        │
                    └──────┬───────┘    └──────▲───────┘
                           │                   │
    ╔══════════════════════╪═══════════════════╪══════════════════════╗
    ║                      ▼                   │       skb path      ║
    ║               ┌──────────────┐    ┌──────────────┐             ║
    ║  TX PATH       │ net_dev_queue│    │netif_receive  │  RX PATH  ║
    ║                │ (tracepoint) │    │   _skb        │           ║
    ║                │              │    │ (tracepoint)  │           ║
    ║                └──────┬───────┘    └──────┬───────┘            ║
    ║                       │                   │                    ║
    ║                ┌──────▼───────┐    ┌──────▼───────┐            ║
    ║                │net_dev_start │    │napi_gro_     │            ║
    ║                │   _xmit      │    │receive_entry │            ║
    ║                │ (tracepoint) │    │ (tracepoint) │            ║
    ║                └──────┬───────┘    └──────┬───────┘            ║
    ║                       │                   │                    ║
    ╚═══════════════════════╪═══════════════════╪════════════════════╝
                            ▼                   │
                    ┌──────────────┐             │
                    │   Driver     │             │
                    │  (e1000)     │             │
                    │  NAPI poll   │             │
                    └──────┬───────┘             │
                           │                    /
                     ┌─────▼─────┐      ┌──────┴──────┐
                     │  TX Ring  │      │   RX Ring   │
                     └─────┬─────┘      └──────┬──────┘
                           │                   │
                    ═══════╪═══════════════════╪══════════
                           ▼                   ▲
                      ┌──────────────────────────┐
                      │      NIC Hardware         │
                      │   (VMware vmxnet3/e1000)  │
                      └──────────────────────────┘

    ═══ 以上为内核网络栈；skb:kfree_skb 可在释放/drop 时触发 ═══
```

### 2.2 skb 生命周期中的 tracepoint

```
  alloc_skb()                     kfree_skb()
      │                                ▲
      ▼                                │
  ┌──────┐    RX PATH     ┌────────┐   │  DROP
  │  skb  │ ────────────► │ 网络栈  │ ──┤
  └──────┘                └───┬────┘   │
      ▲                      │        │
      │              ┌───────▼──────┐  │
      │              │  TX PATH     │──┘
      │              └──────────────┘
      │
  -- tracepoint 挂载点 ------------------
      │
      ├── net:netif_receive_skb       (RX 进入网络栈)
      ├── net:napi_gro_receive_entry  (GRO 合包入口)
      ├── net:net_dev_queue           (skb 入发送队列)
      ├── net:net_dev_start_xmit      (驱动开始发送)
      └── skb:kfree_skb               (skb 释放/drop)
```

### 2.3 kprobe vs tracepoint 对比

| 维度 | kprobe | tracepoint |
|------|--------|------------|
| **机制** | 在函数入口/出口动态插桩，替换第一条指令为 int3/brk | 内核代码中预埋的静态 hook 点（`trace_<name>()` 宏） |
| **稳定性** | 函数名依赖内核版本和编译选项（如 `__napi_poll` vs `napi_poll`） | 内核 ABI，tracepoint 变更视为 ABI break，必须兼容 |
| **字段访问** | 需通过 `arg0`/`arg1` + 寄存器约定 + 强制类型转换访问参数 | `args->field` 直接访问，字段名和类型由 BTF 自动生成 |
| **性能开销** | 函数入口/出口均有开销，每次调用都触发 | 仅在有活跃 tracer 时执行，静态 key 跳过后几乎零开销 |
| **可移植性** | 不同架构/内核版本的函数签名可能不同 | 跨架构、跨版本一致 |
| **适用场景** | 深层路径无 tracepoint 时，或需要观察函数内部逻辑 | skb 路径、调度、syscall 等稳定接口 |
| **在 driver-lab 中** | Phase 2 用于 NAPI poll 深层观测 | Phase 3 用于 skb 路径标准观测 |

### 2.4 为什么 tracepoint 是"内核 ABI"

```text
内核源代码中：
  include/trace/events/net.h:
    TRACE_EVENT(netif_receive_skb,
        TP_PROTO(struct sk_buff *skb),
        TP_ARGS(skb),
        TP_STRUCT__entry(
            __string(name, skb->dev->name)
            __field(unsigned int, len)
        ),
        TP_fast_assign(
            __assign_str(name, skb->dev->name);
            __entry->len = skb->len;
        ),
        TP_printk("dev=%s len=%u", __get_str(name), __entry->len)
    );

用户空间：
  $ sudo bpftrace -l 'tracepoint:net:netif_receive_skb'
  tracepoint:net:netif_receive_skb        ← 内核保证存在

  $ sudo bpftrace -e 'tracepoint:net:netif_receive_skb
      { printf("dev=%s len=%d\n", args->name, args->len); }'
  ← args->name 和 args->len 由 BTF 自动映射
```

而 kprobe 的情况：

```text
5.x 内核 → kprobe:napi_poll          (符号存在)
6.x 内核 → kprobe:napi_poll NOT FOUND  (符号改名或内联)
          → 需要 fallback 到 __napi_poll 或 poll_one_napi

每次内核升级都要重新探测并适配，无法做到"一次写好，永久可用"。
```

### 2.5 bpftrace tracepoint provider 原理

```
┌─────────────────────────────────────────────────────┐
│                    bpftrace                          │
│                                                      │
│  1. 解析脚本                                         │
│     tracepoint:net:netif_receive_skb { ... }         │
│                                                      │
│  2. 查找 tracepoint                                  │
│     /sys/kernel/debug/tracing/events/net/            │
│       netif_receive_skb/                             │
│         id      ← tracepoint ID                      │
│         format  ← 字段定义                            │
│                                                      │
│  3. 生成 BPF 程序                                    │
│     - 读取 format 获取字段偏移                         │
│     - 生成 struct 访问代码                            │
│     - 编译成 BPF bytecode                             │
│                                                      │
│  4. attach 到 perf event                             │
│     perf_event_open(tracepoint_id)                   │
│     ↓                                                │
│     内核触发 trace_netif_receive_skb()                │
│     ↓                                                │
│     BPF 程序执行 → 更新 map → interval 输出           │
└─────────────────────────────────────────────────────┘

关键区别：
- kprobe:   替换函数指令 → 寄存器取值 → 需要知道调用约定
- tracepoint: 静态 hook → 内核写好的字段 → BTF 自动解析
```

## 3. 掌握的知识点

### 3.1 核心概念

| 概念 | 描述 |
|------|------|
| **skb (socket buffer)** | Linux 内核网络栈的核心数据结构，承载网络包的所有信息（数据、协议头、设备） |
| **tracepoint** | 内核中预埋的静态观测点，通过 `TRACE_EVENT` 宏定义，有 ABI 保证 |
| **netif_receive_skb** | skb 进入协议栈的入口 tracepoint，RX 方向 |
| **net_dev_queue** | skb 进入设备发送队列，TX 方向的第一关 |
| **net_dev_start_xmit** | 驱动开始发送 skb，TX 方向的最后一关 |
| **napi_gro_receive_entry** | NAPI 之后 GRO（Generic Receive Offload）尝试合包的入口 |
| **kfree_skb** | skb 被释放，包括正常完成和异常 drop |
| **BTF (BPF Type Format)** | 内核类型信息的自描述格式，bpftrace 用它自动生成字段访问代码 |
| **perf_event_paranoid** | 控制非特权用户访问 perf event 的级别，=4 时需要 root |
| **BEGIN/END 块** | bpftrace 特殊探针，分别在脚本启动和退出时执行一次 |

### 3.2 数据面理解

基于本次测试获得的定量数据：

```
RX 路径:  10 包/秒 (ping -i 0.1 → 每秒 10 个 ICMP echo request)
TX 路径:  ~9.3 包/秒 (ICMP reply + potential ARP)
GRO:      每包都触发 GRO receive entry
          → 因为 ping 小包不入 GRO，但 tracepoint 仍被触发
kfree_skb: ping 正常收发时无异常释放，kfree_skb 计数为 0
           → 0 也是有效观测！说明路径正常，没有 drop

TX 关键指标:
  - net_dev_queue 和 net_dev_start_xmit 计数完全一致
    → 入队的包都被正确发送了
  - hist 显示 100% 包在 [64, 128) 字节范围
    → ping 包 payload 56 + ICMP 8 + IP 20 + ETH 14 = 98 字节 ✓
  - CPU 分布: CPU 0 和 CPU 1 交替处理
    → RPS/RFS 可能在工作，或者 IRQ 亲和性在轮转
```

### 3.3 工程技能

| 技能 | 如何掌握 |
|------|---------|
| 用 bpftrace 列出可用 tracepoint | `bpftrace -l 'tracepoint:net:*'` |
| 判断 tracepoint 是否存在 | 直接列即可，不需要像 kprobe 那样 fallback |
| tracepoint 字段访问 | `args->name`, `args->len` 等，bpftrace 提供补全 |
| 快速手动验证 | `bpftrace -e 'tracepoint:net:netif_receive_skb { printf(...) }'` |
| 多 tracepoint 合并观测 | 一次脚本挂多个 tracepoint，实现全路径关联 |
| histogram 分析 | `hist(args->len)` 自动分桶统计包长度分布 |

## 4. 本 Lab 与整个 track 的关系

```text
track-ebpf-observability 技术栈演进:

  Phase 1: bpftrace + kprobe      ── 快速建立"什么可以被观测"的感觉
  Phase 2: bpftrace + kprobe      ── 深入 NAPI poll 一个点，建立定量思维
  Phase 3: bpftrace + tracepoint  ── 认识到 ABI 的重要性，过渡到稳定接口  ← 当前
  Phase 4: C + libbpf + ringbuf   ── 从脚本到编译型工具，性能提升
  Phase 5: 综合项目              ── 收口成完整网络观测工具

  工具链          Phase 1-3                   Phase 4-5
  ─────────────────────────────────────────────────────
  观测能力 ────── 脚本级 (bpftrace) ──────► 编译型 (libbpf)
  接口类型 ────── kprobe ──► tracepoint ──► tracepoint + kprobe
  数据方式 ────── 终端输出 ───────────────► ringbuf/perfbuf
  可部署性 ────── 开发环境 ───────────────► 生产环境
```

## 5. 流程图总结

```
              ┌─────────────────────────────────────────┐
              │     Phase 3: tracepoint 观测 skb 路径     │
              └─────────────────────────────────────────┘
                                │
              ┌─────────────────┼─────────────────┐
              ▼                 ▼                  ▼
        ┌──────────┐     ┌──────────┐      ┌──────────┐
        │ 02 RX    │     │ 03 TX    │      │ 04 DROP  │
        │ trace    │     │ trace    │      │ trace    │
        └────┬─────┘     └────┬─────┘      └────┬─────┘
             │                │                 │
             ▼                ▼                 ▼
   netif_receive_skb    net_dev_queue      kfree_skb
   napi_gro_receive     net_dev_start_xmit  (基本)
             │                │                 │
             └────────────────┼─────────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │ 05 FULL PATH     │
                    │ (合并观测)        │
                    │                  │
                    │ RX: netif_       │
                    │     receive_skb  │
                    │     + GRO        │
                    │ TX: queue + xmit │
                    │ → 全路径关联     │
                    └──────────────────┘
```

## 6. 参考文献

- Linux 内核源码: `include/trace/events/net.h` (net tracepoint 定义)
- Linux 内核源码: `include/trace/events/skb.h` (skb tracepoint 定义)
- bpftrace 文档: tracepoint provider
- kernel.org: `Documentation/trace/tracepoints.rst`
- Phase 2 本 track: `lab-kprobe-trace-napi-poll/` (kprobe 观测对照)
