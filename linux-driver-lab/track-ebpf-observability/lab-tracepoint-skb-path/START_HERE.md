# START_HERE

当前目标：使用 net tracepoints 观察 receive/xmit/drop。

## 阅读顺序

```text
1. README.md         — lab 概览、tracepoint 列表、运行指令
2. docs/01_GOAL_AND_SCOPE.md  — 目标与范围
3. docs/02_ACCEPTANCE.md      — 验收标准
```

## 运行顺序

```bash
# Step 1: 环境检查
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=10 bash scripts/00_check_env.sh

# Step 2: 列出可用 tracepoint
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=10 bash scripts/01_list_tracepoints.sh

# Step 3: 依次跑各路径观测（另开窗口制造流量）
#   ping -i 0.2 <网关或对端 IP>
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=15 bash scripts/02_run_skb_rx_trace.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=15 bash scripts/03_run_skb_tx_trace.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=15 bash scripts/04_run_skb_drop_trace.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=15 bash scripts/05_run_skb_full_path.sh

# Step 4: 收集统计 + 生成 review
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=10 bash scripts/06_collect_stats.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=10 bash scripts/07_make_review_bundle.sh
```

## 手动探测 tracepoint 可用性

```bash
# 检查目标 tracepoint 是否存在
sudo bpftrace -l 'tracepoint:net:netif_receive_skb'
sudo bpftrace -l 'tracepoint:net:net_dev_start_xmit'
sudo bpftrace -l 'tracepoint:skb:kfree_skb'

# 列出所有 net/skb tracepoint
sudo bpftrace -l 'tracepoint:net:*'
sudo bpftrace -l 'tracepoint:skb:*'

# 快速验证：临时挂一个 tracepoint 看是否有事件
sudo bpftrace -e 'tracepoint:net:netif_receive_skb { printf("RX: dev=%s len=%d\n", args->name, args->len); }'
```
