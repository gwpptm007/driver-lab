# lab-bpftrace-netdev-observe

> eBPF 可观测性第一站：用 bpftrace 观察 Linux 网络 RX/TX/softirq 路径。

## 本版修正点

这版改成 **tracepoint-first**：

```text
RX: tracepoint:net:netif_receive_skb
TX: tracepoint:net:net_dev_queue / tracepoint:net:net_dev_xmit
softirq: tracepoint:irq:softirq_entry / softirq_exit
kprobe: optional only
```

原因：测试机上的 bpftrace 对 `BEGIN/END` block、部分 kprobe/BTF 存在兼容问题。tracepoint 更稳定，适合作为本 lab 的验收路径。

## 推荐测试

```bash
./scripts/00_check_env.sh
./scripts/01_list_probes.sh
sudo EBPF_CONFIRM_XDP_OFF=YES ./scripts/02_clean_xdp_if_attached.sh

sudo ./scripts/03_run_rx.sh
sudo ./scripts/04_run_tx.sh
sudo ./scripts/05_run_softirq.sh
sudo ./scripts/06_run_kprobe.sh

./scripts/07_collect_stats.sh
./scripts/08_make_review.sh
```

## 通过标准

最低通过：

```text
PASS_ENV=YES
PASS_PROBE_LIST=YES
PASS_TRACEPOINT_RX=YES 或 PASS_TRACEPOINT_TX=YES
PASS_SOFTIRQ=YES
REVIEW_BUNDLE.md 生成
```

如果没有真实流量，tracepoint 日志可能存在但计数为 0，此时属于 `PASS_TRACEPOINT_SMOKE`，不是 `PASS_TRAFFIC_OBSERVED`。
