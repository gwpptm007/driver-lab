# START_HERE

当前测试对象是：

```text
track-ebpf-observability/lab-bpftrace-netdev-observe
```

不是独立的 `project-linux-network-observability`。

## 执行顺序

```bash
cd track-ebpf-observability/lab-bpftrace-netdev-observe

./scripts/00_check_env.sh
./scripts/01_list_probe_points.sh
sudo EBPF_CONFIRM_XDP_OFF=YES ./scripts/02_clean_xdp_if_attached.sh

sudo ./scripts/03_run_tracepoint_rx.sh
sudo ./scripts/04_run_tracepoint_tx.sh
sudo ./scripts/05_run_softirq_observe.sh
sudo ./scripts/06_run_optional_kprobe.sh

./scripts/08_collect_stats.sh
./scripts/09_make_review_bundle.sh
```

## 如果 ens192 没有流量

可以先用管理口做 smoke：

```bash
BPFTRACE_IFACE=ens33 sudo ./scripts/03_run_tracepoint_rx.sh
BPFTRACE_IFACE=ens33 sudo ./scripts/04_run_tracepoint_tx.sh
```

然后：

```bash
./scripts/08_collect_stats.sh
./scripts/09_make_review_bundle.sh
```
