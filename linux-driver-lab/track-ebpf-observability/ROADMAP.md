# ROADMAP

## 总体顺序

```text
lab-bpftrace-netdev-observe
    ↓
lab-kprobe-trace-napi-poll
    ↓
lab-tracepoint-skb-path
    ↓
lab-libbpf-net-observer
    ↓
project-linux-network-observability
```

## Phase 1：`lab-bpftrace-netdev-observe`

目标：

```text
用 bpftrace 快速观察 Linux 网络 RX/TX/NAPI/softirq 路径。
```

验收：

```text
ENV_CHECK.txt 存在
PROBE_POINTS.txt 存在
RX_OBSERVE.log / TX_OBSERVE.log 至少一个存在
SOFTIRQ_OBSERVE.log / NAPI_OBSERVE.log 至少一个存在
REVIEW_BUNDLE.md 存在
能够解释 kprobe 与 tracepoint 的适用边界
```

状态：`READY_TO_TEST`

## Phase 2：`lab-kprobe-trace-napi-poll`

目标：

```text
专门围绕 NAPI poll 做观测，包括 napi_poll 调用次数、CPU 分布、软中断关系。
```

状态：`COMPLETED` (2026-05-18, 测试记录见 lab 内 records/)

## Phase 3：`lab-tracepoint-skb-path`

目标：

```text
使用内核 tracepoint 观察 skb receive/xmit/drop 路径，降低 kprobe 函数名变化带来的不稳定。
```

状态：`COMPLETED` (2026-06-06, records 在 lab 内)

## Phase 4：`lab-libbpf-net-observer`

目标：

```text
把前面 bpftrace 验证过的点迁移到 C/libbpf，使用 ringbuf/perfbuf 输出事件。
```

状态：`COMPLETED` (2026-06-06, records 在 lab 内)

## Phase 5：`project-linux-network-observability`

目标：

```text
形成一个项目型网络路径观测工具，能输出 per-interface / per-CPU / RX-TX path report。
```

状态：`COMPLETED` (2026-06-06, records + reports 在 project 内)

## 主线回顾

五个 Phase 构建了完整的 eBPF 网络可观测性技能树：

| 阶段 | 技术栈 | 观测对象 | 核心收获 |
|------|--------|----------|----------|
| Phase 1 | bpftrace + kprobe | netif_rx, net_rx_action | kprobe 快速原型 |
| Phase 2 | bpftrace + kprobe | napi_poll, budget | NAPI/softirq 深度理解 |
| Phase 3 | bpftrace + tracepoint | 5 skb tracepoints | tracepoint ABI 稳定性 |
| Phase 4 | C/libbpf + tracepoint | 同上 + libbpf API | CO-RE, ringbuf, 兼容性 |
| Phase 5 | 统一项目工具 | per-iface/per-CPU/path | 报告, DROP 原因分类 |

从一行 bpftrace 脚本到一个完整的 C/libbpf 编译型观测工具，涵盖了：
- kprobe vs tracepoint 的选择权衡
- BPF CO-RE (vmlinux.h, BTF, bpf_core_read)
- ringbuf 共享内存队列
- per-CPU map 统计
- libbpf 0.5/1.x 版本兼容性
- tracepoint context ABI 手工布局
- __data_loc 字符串解码
- 结构化报告生成
