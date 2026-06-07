# lab-tracepoint-skb-path

> tracepoint 观测 skb path：从 kprobe 函数级 → tracepoint ABI 级

## 目标

使用内核 net/skb tracepoint 观察 skb receive/xmit/drop 完整路径，理解为什么 tracepoint 比 kprobe 更适合做长期可观测性工具。

## 核心 tracepoint

| tracepoint | 含义 | 稳定性 |
|---|---|---|
| `net:netif_receive_skb` | skb 进入网络栈 | 内核 ABI |
| `net:napi_gro_receive_entry` | GRO 合包入口 | 内核 ABI |
| `net:net_dev_queue` | skb 入发送队列 | 内核 ABI |
| `net:net_dev_start_xmit` | 驱动开始发送 | 内核 ABI |
| `skb:kfree_skb` | skb 释放/drop | 内核 ABI |

## 目录

```text
docs/
  01_GOAL_AND_SCOPE.md
  02_ACCEPTANCE.md
scripts/
  00_check_env.sh
  01_list_tracepoints.sh
  02_run_skb_rx_trace.sh
  03_run_skb_tx_trace.sh
  04_run_skb_drop_trace.sh
  05_run_skb_full_path.sh
  06_collect_stats.sh
  07_make_review_bundle.sh
  08_traffic_hint.sh
  09_clean_runtime.sh
records/
reports/
```

## 快速运行

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability/lab-tracepoint-skb-path

sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=10 bash scripts/00_check_env.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=10 bash scripts/01_list_tracepoints.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=10 bash scripts/02_run_skb_rx_trace.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=10 bash scripts/03_run_skb_tx_trace.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=10 bash scripts/04_run_skb_drop_trace.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=10 bash scripts/05_run_skb_full_path.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=10 bash scripts/06_collect_stats.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=10 bash scripts/07_make_review_bundle.sh
```

制造流量（另开窗口）：

```bash
ping -i 0.2 <网关或对端 IP>
```

## 与 Phase 2 的对照

| 维度 | Phase 2 (kprobe) | Phase 3 (tracepoint) |
|---|---|---|
| 观测方式 | `kprobe:__napi_poll` | `tracepoint:net:netif_receive_skb` |
| 稳定性 | 函数名可能因内核版本变化 | 内核 ABI，跨版本不变 |
| 字段访问 | 需知道结构体布局 | `args->name` 直接访问 |
| 适用层 | NAPI poll 层 | skb 路径层 |

## 当前结论

2026-06-06 在 VMware Ubuntu 测试机上复测通过：

```text
PASS_SKB_TRACEPOINT_OBSERVE
kernel: 6.8.0-111-generic
bpftrace: v0.14.0
observed tracepoints:
  net:netif_receive_skb        — RX ✓
  net:napi_gro_receive_entry   — GRO ✓
  net:net_dev_queue            — TX-Q ✓
  net:net_dev_start_xmit       — TX-XMIT ✓
  skb:kfree_skb                — drop ✓ (basic)
```

详细过程见 [records/20260606-161506-tracepoint-skb-path/REVIEW_BUNDLE.md](records/20260606-161506-tracepoint-skb-path/REVIEW_BUNDLE.md) 和 [reports/report.md](reports/report.md)。

## 环境注意事项

- **bpftrace v0.14.0 + kernel 6.8**: BEGIN/END 块不支持（BEGIN_trigger 错误），脚本已适配
- **BTF 限制**: `args->name` 显示为指针值而非字符串；`enum skb_drop_reason` 不完整
- 以上限制不影响核心观测功能

## 下一站

本 lab 已经完成 Phase 3。按照主线，下一站是 Phase 4：

```text
lab-libbpf-net-observer
```

Phase 4 将把 bpftrace 验证过的观测点迁移到 C/libbpf + ringbuf，实现编译型网络观测工具。
