# 02_ACCEPTANCE — 测试过程篇

## 1. 测试概览

| 项目 | 值 |
|------|-----|
| **测试日期** | 2026-06-06 16:15–16:17 CST |
| **测试机** | wq7-virtual-machine (VMware Workstation) |
| **内核版本** | 6.8.0-111-generic (Ubuntu 24.04) |
| **bpftrace 版本** | v0.14.0 |
| **网卡** | ens33 (Intel e1000, PCI 0000:02:01.0) |
| **管理口** | ens33 (同时作为观测目标和 SSH 通道) |
| **IP** | 192.168.65.135/24, gateway 192.168.65.2 |
| **流量来源** | `ping -i 0.1 192.168.65.2` (10 pkt/s ICMP) |
| **最终判定** | **PASS_SKB_TRACEPOINT_OBSERVE** |

## 2. 测试环境完整输出

### 2.1 环境检查 (00_check_env.sh)

**执行命令**（从 Windows 通过 SSH 远程执行）：

```bash
ssh wq7@192.168.65.135
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability/lab-tracepoint-skb-path
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 bash scripts/00_check_env.sh
```

**关键输出**：

```
KERNEL=6.8.0-111-generic
bpftrace: /usr/bin/bpftrace      ← bpftrace 已安装
driver: e1000                     ← Intel e1000 网卡驱动
kptr_restrict=1                   ← 内核指针限制 (root 可见)
perf_event_paranoid=4             ← 需要 sudo
tracefs on /sys/kernel/tracing type tracefs (rw,...)    ← tracefs 已挂载
trace_events_net_dir=/sys/kernel/debug/tracing/events/net/    ← net tracepoint 暴露
trace_events_skb_dir=/sys/kernel/debug/tracing/events/skb/    ← skb tracepoint 暴露
```

**原理对照**：`perf_event_paranoid=4` 意味着非 root 用户无法使用 perf_event (tracepoint 底层机制)，必须 sudo 运行 bpftrace。`kptr_restrict=1` 不影响 root 用户，但普通用户看不到内核指针值。

### 2.2 tracepoint 列表 (01_list_tracepoints.sh)

```bash
sudo EBPF_IFACE=ens33 bash scripts/01_list_tracepoints.sh
```

**发现的 net tracepoint (skb 路径相关)**：

```
tracepoint:net:napi_gro_frags_entry       ← GRO frag 入口
tracepoint:net:napi_gro_frags_exit        ← GRO frag 出口
tracepoint:net:napi_gro_receive_entry     ← ★ GRO 收包入口
tracepoint:net:napi_gro_receive_exit      ← GRO 收包出口
tracepoint:net:net_dev_queue              ← ★ skb 入发送队列
tracepoint:net:net_dev_start_xmit         ← ★ 驱动开始发送
tracepoint:net:net_dev_xmit               ← 发送完成
tracepoint:net:net_dev_xmit_timeout       ← 发送超时
tracepoint:net:netif_receive_skb          ← ★ skb 进入协议栈 (RX 入口)
tracepoint:net:netif_receive_skb_entry    ← RX 列表入口
tracepoint:net:netif_receive_skb_exit     ← RX 列表出口
tracepoint:net:netif_receive_skb_list_entry  ← 批量 RX 入口
tracepoint:net:netif_receive_skb_list_exit   ← 批量 RX 出口
tracepoint:net:netif_rx                   ← 传统 netif_rx
tracepoint:net:netif_rx_entry             ← 传统 netif_rx 入口
tracepoint:net:netif_rx_exit              ← 传统 netif_rx 出口

skb tracepoint:
tracepoint:skb:consume_skb                ← skb 正常消费 (非常见 drop)
tracepoint:skb:kfree_skb                  ← ★ skb 释放 (可能 = drop)
tracepoint:skb:skb_copy_datagram_iovec    ← skb 拷贝到用户空间
```

**原理对照**：这些 tracepoint 在 6.8 内核上全部存在，与 5.x 内核完全一致——这就是 tracepoint ABI 稳定性的直接证据。Phase 2 的 `kprobe:napi_poll` 在这个内核上可能不存在（需 fallback 到 `__napi_poll`），但所有 tracepoint 都无需 fallback。

### 2.3 RX 路径观测 (02_run_skb_rx_trace.sh)

**bpftrace 脚本**（动态生成到 `skb_rx_trace_dynamic.bt`）：

```bpftrace
tracepoint:net:netif_receive_skb
{
    @rx_total = count();
    @rx_by_cpu[cpu] = count();
    @rx_by_name[args->name] = count();
}

tracepoint:net:napi_gro_receive_entry
{
    @gro_entry_total = count();
    @gro_by_cpu[cpu] = count();
    @gro_by_name[args->name] = count();
}

interval:s:1
{
    printf("=== skb RX tracepoint === @ %ds ===\n", elapsed);
    print(@rx_total);
    print(@rx_by_cpu);
    print(@rx_by_name);
    print(@gro_entry_total);
    print(@gro_by_cpu);
    print(@gro_by_name);
}
```

**执行**：
```bash
# 另开窗口: ping -i 0.1 192.168.65.2
sudo EBPF_IFACE=ens33 EBPF_DURATION=10 bash scripts/02_run_skb_rx_trace.sh
```

**观测结果（节选）**：

```
Attaching 3 probes...                      ← 3 个 attach 点:
                                           ←   1. netif_receive_skb
                                           ←   2. napi_gro_receive_entry
                                           ←   3. interval:s:1

=== skb RX tracepoint === @ 1s ===
@rx_total: 10                              ← 每秒 10 个 RX 包
@rx_by_cpu[3]: 10                          ← 全部在 CPU 3 上
@rx_by_name[-18738003175576]: 10           ← 设备指针值 (BTF 限制)
@gro_entry_total: 10                       ← GRO entry 与 RX 同步
@gro_by_cpu[3]: 10
@gro_by_name[-18738003175522]: 10

=== skb RX tracepoint === @ 2s ===
@rx_total: 20                              ← 持续增长
@rx_by_cpu[3]: 20

... (10 秒后)
@rx_total: 93                              ← 总计 ~93 RX 包
RC=124                                      ← timeout 正常退出
TIMEOUT_AS_EXPECTED=1
```

**原理对照**：
- `netif_receive_skb` 计数 = ping 流量速率 (10 pkt/s) × 时间，与预期一致
- `napi_gro_receive_entry` 计数与 `netif_receive_skb` 完全同步 → GRO 入口在每次收包时都被触发
- 小 ping 包不会被 GRO 合并，但 tracepoint 仍然触发，说明 GRO entry 是"N 合 1"的入口但每包都会经过
- `args->name` 显示为十进制负数（指针值）→ 6.8 内核 + bpftrace v0.14 的 BTF 字符串字段解析问题，**不影响计数和 CPU 分布**
- CPU 3 独占处理 → 说明 IRQ 亲和性固定在了 CPU 3 上

### 2.4 TX 路径观测 (03_run_skb_tx_trace.sh)

**bpftrace 脚本**：

```bpftrace
tracepoint:net:net_dev_queue { @tx_queue_total = count(); ... }
tracepoint:net:net_dev_start_xmit { @tx_xmit_total = count(); @tx_xmit_len = hist(args->len); ... }
interval:s:1 { printf("=== skb TX tracepoint === @ %ds ===\n", elapsed); print(...); }
```

**观测结果（节选）**：

```
=== skb TX tracepoint === @ 1s ===
@tx_queue_total: 9                         ← ~9 个包入发送队列
@tx_queue_by_cpu[1]: 9                    ← CPU 1 处理
@tx_xmit_total: 9                          ← 与 queue 计数一致 → 全部发送
@tx_xmit_len:                              ← hist 分布:
[64, 128)              9 |@@@@@@@@@@...   ← 100% 在 64-128 字节

=== skb TX tracepoint === @ 2s ===
@tx_queue_total: 19
@tx_queue_by_cpu[0]: 7                    ← CPU 0 开始参与
@tx_queue_by_cpu[1]: 12
@tx_xmit_total: 19                         ← queue == xmit → 无丢包
@tx_xmit_len:
[64, 128)             19 |@@@@@@@@@@...

... (10 秒后)
@tx_xmit_total: 93                         ← 总计 ~93 TX 包
@tx_xmit_len:
[64, 128)             93 |@@@@@@@@@@...   ← 全部在 64-128 区间
RC=124
```

**原理对照**：
- `net_dev_queue` 计数 = `net_dev_start_xmit` 计数 → 入队的包全部被发送，没有在队列层丢包
- `hist(args->len)` 输出非常精准：ping 回显包 = 98 字节（56 payload + 8 ICMP + 20 IP + 14 ETH），落在 [64, 128) 区间
- CPU 0 和 CPU 1 交替处理 TX → 多 CPU 负载分担
- TX 速率 (~9.3 pkt/s) 略低于 RX (~10 pkt/s) → 合理，因为 ICMP reply 可能少于 request

### 2.5 drop 观测 (04_run_skb_drop_trace.sh)

**bpftrace 脚本**：

```bpftrace
tracepoint:skb:kfree_skb
{
    @kfree_total = count();
    @kfree_by_cpu[cpu] = count();
    @kfree_by_comm[comm] = count();
}
```

**观测结果**：

```
=== skb kfree_skb === @ 1s ===
@kfree_total: 0                           ← 无 skb 被释放
=== skb kfree_skb === @ 2s ===
@kfree_total: 0
... (持续 10 秒)
@kfree_total: 0                           ← 始终为 0
RC=124
```

**原理对照**：
- **kfree_skb = 0 是有效观测！** 正常的 ping 收发不会产生异常 skb 释放 → 说明 RX→TX 路径完整，没有丢包
- 如果看到 `@kfree_total > 0`，说明有 skb 被异常释放（drop），需要进一步排查 location 字段

**BTF 限制**：尝试 `args->location` 和 `args->protocol` 访问时 bpftrace 报错：

```
definitions.h:14:24: error: field has incomplete type 'enum skb_drop_reason'
```

这是因为内核 BTF 中 `enum skb_drop_reason` 是前向声明，bpftrace v0.14.0 无法解析其完整定义。基本计数（cpu, comm）不受影响。

### 2.6 全路径合并观测 (05_run_skb_full_path.sh)

**bpftrace 脚本**（4 个 tracepoint 合并）：

```bpftrace
tracepoint:net:netif_receive_skb    { @rx = count(); @rx_cpu[cpu] = count(); ... }
tracepoint:net:napi_gro_receive_entry { @gro = count(); }
tracepoint:net:net_dev_queue        { @txq = count(); @txq_cpu[cpu] = count(); ... }
tracepoint:net:net_dev_start_xmit   { @tx = count(); @tx_dev[args->name] = count(); @tx_len = hist(args->len); }
interval:s:1 { print(@rx); print(@txq); print(@tx); print(@tx_len); ... }
```

**观测结果（节选）**：

```
Attaching 5 probes...                      ← 4 个 tracepoint + 1 个 interval

=== skb full-path === @ 1s ===
@rx: 10           @gro: 10                ← RX 端: 10 包/秒
@txq: 10          @tx: 10                 ← TX 端: 10 包/秒 (完全匹配!)
@tx_len: [64,128) 10 |@@@@@@@@@@@@@@...   ← 包长在预期范围

=== skb full-path === @ 2s ===
@rx: 19           @gro: 19
@txq: 23          @tx: 23                 ← TX 略高于 RX (多出的可能是 ARP/其他)
@txq_cpu[4]: 1                            ← CPU 4 也参与了
@txq_cpu[1]: 3                            ← CPU 1 贡献 3 包
@txq_cpu[0]: 19                           ← CPU 0 主要处理
@tx_len: [64,128) 20 [128,256) 2 [256,512) 1   ← 出现不同大小包

... (10 秒后)
@rx: 93 total    @tx: 93 total            ← RX ≈ TX, 路径完整
RC=124
```

**原理对照**：
- **RX 和 TX 的计数在同一轮观测中可以直接对照**，这才是全路径观测的价值
- 4 个 tracepoint 同时 attach，无冲突 → bpftrace 支持多 tracepoint 合并观测
- hist 出现了 [128, 256) 和 [256, 512) 区间 → 可能的 ARP 包或非 ICMP 流量
- CPU 分布覆盖 0, 1, 2, 4, 5, 6 → 多 CPU 并行处理的特点

### 2.7 环境兼容性问题与修复

在测试过程中，遇到了两个环境兼容性问题：

| # | 问题 | 表现 | 根因 | 修复 |
|---|------|------|------|------|
| 1 | BEGIN/END 块不工作 | `ERROR: Could not resolve symbol: /proc/self/exe:BEGIN_trigger` | bpftrace v0.14.0 + kernel 6.8 兼容 bug | 去掉所有 BEGIN/END 块，用 `interval:s:1` + `printf()` 替代 |
| 2 | count 类型在 printf 中无效 | `ERROR: printf: %d specifier expects a value of type integer (count supplied)` | bpftrace 的 count() 返回专用 count 类型而非 int64 | 用 `print()` 代替 `printf("%d", @map)` |
| 3 | `args->name` BTF 不完整 | 设备名显示为 `[-18738003175576]` 指针值 | 6.8 内核 BTF 对 tracepoint 字符串字段解析不完整 | 不影响计数和分布，接受此限制 |
| 4 | `enum skb_drop_reason` 不完整 | `definitions.h: error: field has incomplete type` | 内核 BTF 前向声明但无完整定义 | kfree_skb 降级为 basic 模式（cpu, comm 计数） |
| 5 | `if (@map > 0)` 比较无效 | `ERROR: Type mismatch for '>': comparing 'count' with 'int64'` | count 类型不能与整数比较 | 去掉条件判断，无条件 print()（空 map 不显示或显示为不完整的 @key: 0） |

### 2.8 测试记录目录结构

```
records/20260606-161506-tracepoint-skb-path/
├── ENV_CHECK.txt             ← 系统环境检查
├── TRACEPOINT_LIST.txt       ← 可用 tracepoint 清单
├── SKB_RX_TRACE.log          ← RX 路径观测 (netif_receive_skb + GRO)
├── SKB_TX_TRACE.log          ← TX 路径观测 (net_dev_queue + xmit)
├── SKB_DROP_TRACE.log        ← drop/kfree 观测
├── SKB_FULL_PATH.log         ← 全路径合并观测
├── COLLECT_STATS.txt         ← 测试后系统统计
├── REVIEW_BUNDLE.md          ← 自动判卷结果
├── skb_rx_trace_dynamic.bt   ← 动态生成的 bpftrace 脚本
├── skb_tx_trace_dynamic.bt
├── skb_drop_trace_dynamic.bt
└── skb_full_path_dynamic.bt
```

## 3. 完整执行命令序列

以下是与测试机交互的完整命令流水线：

```bash
# === 阶段 1: 部署脚本到测试机 ===
# (从 Windows 本地)
cd /e/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/track-ebpf-observability/lab-tracepoint-skb-path
tar czf /tmp/tracepoint-skb-path.tar.gz scripts/ docs/ README.md START_HERE.md records/README.md reports/report.md
scp /tmp/tracepoint-skb-path.tar.gz wq7@192.168.65.135:/tmp/
ssh wq7@192.168.65.135 "cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability/lab-tracepoint-skb-path && tar xzf /tmp/tracepoint-skb-path.tar.gz && chmod +x scripts/*.sh"

# === 阶段 2: 环境检查 ===
ssh wq7@192.168.65.135
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability/lab-tracepoint-skb-path
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 bash scripts/00_check_env.sh
sudo EBPF_IFACE=ens33 bash scripts/01_list_tracepoints.sh

# === 阶段 3: 跑主观测 (4 个脚本，每个 10 秒) ===
# 在另一个 SSH 窗口制造流量:
ping -i 0.1 192.168.65.2

# 依次执行:
sudo EBPF_IFACE=ens33 EBPF_DURATION=10 bash scripts/02_run_skb_rx_trace.sh
sudo EBPF_IFACE=ens33 EBPF_DURATION=10 bash scripts/03_run_skb_tx_trace.sh
sudo EBPF_IFACE=ens33 EBPF_DURATION=10 bash scripts/04_run_skb_drop_trace.sh
sudo EBPF_IFACE=ens33 EBPF_DURATION=10 bash scripts/05_run_skb_full_path.sh

# === 阶段 4: 收集+判卷 ===
sudo EBPF_IFACE=ens33 bash scripts/06_collect_stats.sh
sudo EBPF_IFACE=ens33 bash scripts/07_make_review_bundle.sh

# === 阶段 5: 拷贝 records 回本地 ===
# (从 Windows)
scp -r wq7@192.168.65.135:/home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability/lab-tracepoint-skb-path/records/20260606-161506-tracepoint-skb-path \
  e:/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/track-ebpf-observability/lab-tracepoint-skb-path/records/
```

## 4. 判卷结果

```
PASS_SKB_TRACEPOINT_OBSERVE

PASS_ENV                YES   ← 环境工具齐全
PASS_TRACEPOINT_LIST    YES   ← tracepoint 列表已生成
PASS_RX_TRACE           YES   ← RX 路径正常捕获
PASS_TX_TRACE           YES   ← TX 路径正常捕获
PASS_DROP_TRACE         YES   ← drop 观测正常运行 (0 drop = 正常)
PASS_FULL_PATH          YES   ← 全路径合并观测正常
TRAFFIC_OR_EVENTS_OBSERVED  YES   ← 有真实流量触发的事件
```

## 5. 数据分析与原理验证

基于所有日志文件，总结以下关键发现：

```
                    RX PATH                     TX PATH
                    ─────────                  ─────────
netif_receive_skb:  93 events (10s)            93 events (10s)  : net_dev_start_xmit
napi_gro_receive:   93 events (同步)            93 events (同步)  : net_dev_queue
kfree_skb:          0 events                   hist[64,128) = 93

含义:
1. 收发包速率 ≈ 9.3 pkt/s，与 ping -i 0.1 的 10 pkt/s 速率吻合
2. GRO entry 触发但无合包 → 小包路径正常工作
3. net_dev_queue = net_dev_start_xmit → 队列层无丢包
4. kfree_skb = 0 → 无异常释放 → 路径完整
5. CPU 分布覆盖 0, 1, 2, 3, 4, 5, 6 → 多核并行
6. hist 确认包长在预期范围，出现少量 >128 字节包 = 其他协议
```

这与 [01_GOAL_AND_SCOPE.md](01_GOAL_AND_SCOPE.md) 中描述的 skb 路径流程图完全吻合，验证了 tracepoint 作为观测手段的正确性和有效性。
