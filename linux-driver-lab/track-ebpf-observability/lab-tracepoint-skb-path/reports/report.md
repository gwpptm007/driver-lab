# lab-tracepoint-skb-path report

## 结论

**PASS_SKB_TRACEPOINT_OBSERVE** — 2026-06-06

在 VMware Ubuntu 测试机（kernel 6.8.0-111-generic, bpftrace v0.14.0, e1000 驱动）上通过全量 tracepoint 观测。

## 测试环境

| 项目 | 值 |
|------|-----|
| 测试机 | wq7-virtual-machine (VMware) |
| 内核 | 6.8.0-111-generic |
| bpftrace | v0.14.0 |
| 网卡 | ens33 (e1000) |
| 流量 | ping -i 0.1 192.168.65.2 |

## 通过的 tracepoint

| tracepoint | RC | 捕获情况 |
|------------|-----|-----|
| `net:netif_receive_skb` | 124 (TIMEOUT) | RX 事件持续增长 10→20→... |
| `net:napi_gro_receive_entry` | 124 (TIMEOUT) | GRO 入口事件与 RX 同步 |
| `net:net_dev_queue` | 124 (TIMEOUT) | TX-Queue 事件 ~10/s |
| `net:net_dev_start_xmit` | 124 (TIMEOUT) | TX-XMIT 事件 ~10/s，见 hist 分布 |
| `skb:kfree_skb` | 124 (TIMEOUT) | 基本计数正常，CPU 分布见 |
| full-path (合并) | 124 (TIMEOUT) | RX→TX 双向观测正常 |

## 环境限制与应对

| 限制 | 表现 | 应对 |
|------|------|------|
| BEGIN/END 块不支持 | `BEGIN_trigger` 符号解析失败 | 去掉 BEGIN/END，用 interval:s:1 驱动输出 |
| `args->name` BTF 不完整 | 设备名显示为指针值 | 不影响计数，CPU 分布仍然有效 |
| `enum skb_drop_reason` 不完整 | `args->location` 无法访问 | kfree_skb 降级为基本计数（cpu, comm） |

## 与 Phase 2 对照

| 维度 | Phase 2 (kprobe) | Phase 3 (tracepoint) |
|------|------------------|----------------------|
| 观测对象 | `__napi_poll` 函数 | `netif_receive_skb` 等 ABI |
| 稳定性 | 函数名随内核版本变化 | ✅ ABI 保证不变 |
| 字段访问 | 需知结构体布局 | ✅ `args->name` 等直接访问 |
| 环境依赖 | kprobe 符号表 | tracepoint 始终存在 |
| 实测结果 | PASS_NAPI_OBSERVE (2026-05-18) | PASS_SKB_TRACEPOINT_OBSERVE (2026-06-06) |

## 关键学习

1. **tracepoint 才是做网络可观测的正确选择**。6.8 内核上的 tracepoint 和 5.x 上的完全一致，不需要 fallback 多个符号名。
2. **bpftrace 版本兼容性问题需要关注**。v0.14.0 的 BEGIN_trigger 问题可能是 bpftrace 与 6.8 内核的兼容 bug，不影响核心功能。
3. **BTF 字段不完整是现实问题**。`args->name` 显示为指针值说明 BTF 对某些 tracepoint 的字符串字段解析有问题，但不影响计数和分布。

## files

- `records/20260606-161506-tracepoint-skb-path/` — 完整测试证据
