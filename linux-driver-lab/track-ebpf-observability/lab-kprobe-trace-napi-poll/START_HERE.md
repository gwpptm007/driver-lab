# START_HERE

这一站测试 NAPI poll 观测。

## 一键顺序

```bash
cd track-ebpf-observability/lab-kprobe-trace-napi-poll

./scripts/00_check_env.sh
./scripts/01_list_napi_probe_points.sh
sudo ./scripts/02_run_napi_poll_kprobe.sh
sudo ./scripts/03_run_napi_poll_retprobe.sh
sudo ./scripts/04_run_driver_poll_probe.sh
sudo ./scripts/05_run_softirq_napi_correlation.sh
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```

## 如果没有事件

先确认目标网卡有没有流量：

```bash
./scripts/08_traffic_hint.sh
```

然后在另一个窗口 ping/iperf/ssh 产生流量。

## 注意

本 lab 使用 kprobe/kretprobe。不同内核、不同编译选项、不同安全策略下，部分函数可能不可附加。`driver poll probe` 是增强项，不作为最低 PASS 的硬性条件。
