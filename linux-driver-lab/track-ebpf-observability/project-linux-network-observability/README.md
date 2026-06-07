# project-linux-network-observability

> Phase 5: Linux 网络可观测性统一项目

## 定位

整合 Phase 1-4 的观测能力（bpftrace kprobe → bpftrace tracepoint → C/libbpf 编译型工具），形成一个可复用的网络路径观测项目工具。

## 功能

| 功能 | 说明 |
|------|------|
| **5 个内核 tracepoint** | netif_receive_skb, napi_gro_receive_entry, net_dev_queue, net_dev_start_xmit, kfree_skb |
| **per-interface 统计** | 按网卡 (ens33/ens34/lo) 分类 RX/TX/DROP/GRO |
| **per-CPU 分布** | 从 BPF per-CPU map 读取，展示 CPU 负载分布 |
| **DROP 原因分类** | 从 kfree_skb 捕获 skb_drop_reason，分类展示 |
| **路径分析** | RX→GRO, TX-QUEUE→TX-XMIT 不变量验证，丢包率计算 |
| **Markdown 报告** | `-o report.md` 输出结构化报告 |

## 架构

```text
src/net_observer.bpf.c  →  BPF 内核程序 (5 tracepoint handlers)
src/net_observer.c      →  Userspace 加载器 + 报告生成器
src/net_observer.h      →  共享定义 (事件结构, 统计结构)
```

## 快速开始

```bash
# 1. 编译
bash scripts/01_build.sh

# 2. 运行 (控制台输出)
sudo build/net_observer -v -d 15

# 3. 运行 + 生成报告
sudo build/net_observer -v -d 15 -o reports/observe-$(date +%Y%m%d-%H%M%S).md

# 4. 一键运行 + 报告 (脚本)
EBPF_DURATION=30 bash scripts/03_generate_report.sh
```

## CLI 参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `-d SEC` | 10 | 运行时长 (0=直到 Ctrl+C) |
| `-v` | off | 详细模式 (打印每个事件) |
| `-o REPORT.md` | (无) | 输出 markdown 报告文件 |
| `-i IFACE` | (全部) | 仅过滤指定接口 |
| `-h` | | 帮助 |

## 与 Phase 1-4 的关系

```text
Phase 1: bpftrace + kprobe     → 单点观测原型
Phase 2: bpftrace + kprobe     → NAPI poll 深度观测
Phase 3: bpftrace + tracepoint → ABI 稳定观测
Phase 4: C/libbpf + tracepoint → 编译型观测工具
Phase 5: ★ 收口成统一项目       → per-iface + per-CPU + 报告
```

## 依赖

- clang (>= 12)
- bpftool
- libbpf-dev (>= 0.5)
- gcc, make
- Linux kernel >= 5.10 (with BTF)
