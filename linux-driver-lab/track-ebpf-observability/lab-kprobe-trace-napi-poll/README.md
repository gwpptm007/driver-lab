# lab-kprobe-trace-napi-poll

本 lab 聚焦一个问题：用 bpftrace 观察 Linux 网络收包路径里的 NAPI poll。

它不是性能压测工具，也不是完整网络栈分析项目。它要证明三件事：

1. 当前内核里哪个 NAPI poll 符号可以被 kprobe/kretprobe 观测。
2. `NET_RX` softirq 和 NAPI poll 是否能在同一轮测试里关联起来。
3. 测试记录是否足够清楚，能解释为什么 PASS、WARN 或需要重测。

## 目录

```text
docs/
  01_learning_notes_and_principles.md
  02_project_review_and_deep_dive.md
  03_test_record_20260518_vm.md
scripts/
  00_check_env.sh
  01_list_napi_probe_points.sh
  02_run_napi_poll_kprobe.sh
  03_run_napi_poll_retprobe.sh
  04_run_driver_poll_probe.sh
  05_run_softirq_napi_correlation.sh
  06_collect_stats.sh
  07_make_review_bundle.sh
  08_traffic_hint.sh
  09_clean_runtime.sh
records/
  20260518-214641-kprobe-trace-napi-poll/
reports/
  report.md
```

`probes/` 已删除。现在 probe 脚本由运行脚本动态生成到本轮 `records/` 目录，原因是不同内核上 `napi_poll` 不一定可 trace，固定 `.bt` 文件容易制造假失败或假通过。

## 快速运行

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability/lab-kprobe-trace-napi-poll

sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=5 bash scripts/00_check_env.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=5 bash scripts/01_list_napi_probe_points.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=5 bash scripts/02_run_napi_poll_kprobe.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=5 bash scripts/03_run_napi_poll_retprobe.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=5 bash scripts/04_run_driver_poll_probe.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=5 bash scripts/05_run_softirq_napi_correlation.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=5 bash scripts/06_collect_stats.sh
sudo EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=5 bash scripts/07_make_review_bundle.sh
```

没有流量时，另开一个窗口制造流量，例如：

```bash
ping -i 0.2 <网关或对端 IP>
```

## 当前结论

2026-05-18 在 VMware Ubuntu 测试机上复测通过：

```text
PASS_NAPI_OBSERVE
selected kprobe:    kprobe:__napi_poll
selected kretprobe: kretprobe:__napi_poll
NET_RX softirq:     observed
NAPI poll calls:    observed
```

详细过程见 [docs/03_test_record_20260518_vm.md](docs/03_test_record_20260518_vm.md) 和 [reports/report.md](reports/report.md)。

## 下一站

本 lab 已经完成 Phase 2：NAPI poll 的 kprobe/kretprobe 观测。按照 `track-ebpf-observability/README.md` 的主线，下一站是 Phase 3：

```text
lab-tracepoint-skb-path
```

下一站会从函数级 kprobe 观察，推进到 skb 路径上的 tracepoint 观察，重点回答：

```text
1. skb 在 RX/TX 路径上经过哪些稳定 tracepoint。
2. tracepoint 相比 kprobe 为什么更稳定。
3. 如何用 tracepoint 观察 skb 的协议、设备、长度、方向。
4. 如何把本 lab 的 NAPI/softirq 证据继续串到 skb 层路径。
```

也就是说，本 lab 解决的是“包什么时候进入 NAPI poll”；下一站要看“进入内核网络栈后，skb 在哪些 tracepoint 上可被稳定观察”。`XDP / AF_XDP` 会放在更后面的数据面专题里学习。
