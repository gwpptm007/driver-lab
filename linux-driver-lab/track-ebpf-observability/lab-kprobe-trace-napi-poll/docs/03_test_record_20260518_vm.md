# 测试记录：2026-05-18 VMware Ubuntu

## 测试背景

本地 Windows 工作区完成脚本修正后，需要在真实 Linux 内核上验证。用户启动 VMware Ubuntu 测试机，并提供：

```text
IP: 192.168.65.135
用户: wq7
测试路径: /home/wq7/workspace/driver-lab/linux-driver-lab
```

测试目标是确认：

```text
1. 修正后的脚本能在 Linux 上通过 bash 语法检查。
2. 动态 probe 选择能绕过不可 trace 的 napi_poll。
3. bpftrace 能真实 attach 并产生 NAPI/softirq 事件。
4. review bundle 不再误判 attach failure。
```

## 远程连接与目录准备

先确认 SSH 和内核：

```bash
ssh -o StrictHostKeyChecking=no wq7@192.168.65.135 "pwd; hostname; uname -a"
```

结果：

```text
/home/wq7
wq7-virtual-machine
Linux wq7-virtual-machine 6.8.0-111-generic ... x86_64 GNU/Linux
```

检查用户指定路径后发现，VM 中尚无 `track-ebpf-observability/lab-kprobe-trace-napi-poll`。因此先创建目录，并把本地修正后的 lab 同步过去：

```bash
ssh wq7@192.168.65.135 "mkdir -p /home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability"

scp -r linux-driver-lab/track-ebpf-observability/lab-kprobe-trace-napi-poll \
  wq7@192.168.65.135:/home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability/
```

## 预检查

进入 VM 后执行语法检查：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability/lab-kprobe-trace-napi-poll

bash -n \
  scripts/common.sh \
  scripts/02_run_napi_poll_kprobe.sh \
  scripts/03_run_napi_poll_retprobe.sh \
  scripts/05_run_softirq_napi_correlation.sh \
  scripts/07_make_review_bundle.sh
```

结果：无输出，表示语法检查通过。

检查工具和 sudo：

```bash
sudo -n true && echo SUDO_NOPASSWD=1 || echo SUDO_NEEDS_PASSWORD=1
which bpftrace
bpftrace --version
```

结果：

```text
SUDO_NEEDS_PASSWORD=1
/usr/bin/bpftrace
bpftrace v0.14.0
```

检查网卡：

```bash
ip -br link
ip -br addr
ethtool -i ens192
ethtool -i ens33
```

关键结果：

```text
ens192 UP 192.168.100.1/24 driver: vmxnet3
ens33  UP 192.168.65.135/24 driver: e1000
```

因为 SSH 和可制造流量的路径在 `ens33`，本轮选择 `ens33` 做 smoke test。

## 实际执行

测试命令：

```bash
sudo env EBPF_IFACE=ens33 EBPF_MGMT_IFACE=ens33 EBPF_DURATION=5 bash -lc '
  cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability/lab-kprobe-trace-napi-poll
  (ping -i 0.2 -c 80 192.168.65.2 >/dev/null 2>&1 &)
  bash scripts/00_check_env.sh
  bash scripts/01_list_napi_probe_points.sh
  bash scripts/02_run_napi_poll_kprobe.sh
  bash scripts/03_run_napi_poll_retprobe.sh
  bash scripts/04_run_driver_poll_probe.sh
  bash scripts/05_run_softirq_napi_correlation.sh
  bash scripts/06_collect_stats.sh
  bash scripts/07_make_review_bundle.sh
'
```

生成记录目录：

```text
records/20260518-214641-kprobe-trace-napi-poll/
```

## 关键日志

`NAPI_POLL_KPROBE.log`：

```text
selected probe:
kprobe:__napi_poll

@napi_poll_calls[kprobe:__napi_poll]: 37
RC=124
TIMEOUT_AS_EXPECTED=1
```

`NAPI_POLL_RETPROBE.log`：

```text
selected probe:
kretprobe:__napi_poll

@napi_poll_returns[kretprobe:__napi_poll, 0]: 11
@napi_poll_returns[kretprobe:__napi_poll, 1]: 27
RC=124
TIMEOUT_AS_EXPECTED=1
```

`SOFTIRQ_NAPI_CORRELATION.log`：

```text
@net_rx_softirq_entry: 4
@net_rx_softirq_exit: 4
@napi_poll_calls[kprobe:__napi_poll]: 4
RC=124
TIMEOUT_AS_EXPECTED=1
```

`DRIVER_POLL_OPTIONAL.log`：

```text
selected probes:
kprobe:vmxnet3_poll
kprobe:napi_complete_done
kprobe:napi_gro_receive

@driver_or_napi_helpers[kprobe:napi_complete_done]: 4
RC=124
TIMEOUT_AS_EXPECTED=1
```

`REVIEW_BUNDLE.md`：

```text
PASS_ENV: YES
PASS_PROBE_LIST: YES
PASS_NAPI_KPROBE: YES
PASS_NAPI_RETPROBE: YES
DRIVER_POLL_OPTIONAL: YES
PASS_SOFTIRQ_CORRELATION: YES
TRAFFIC_OR_EVENTS_OBSERVED: YES

PASS_NAPI_OBSERVE
```

## 测试结论

本轮测试证明：

```text
1. 固定 napi_poll 不是可靠策略。
2. 当前内核可观测入口是 __napi_poll。
3. kprobe 和 kretprobe 都成功 attach。
4. NET_RX softirq 和 __napi_poll 能在同一轮测试中关联出现。
5. review bundle 的 PASS 有原始日志支撑。
```

因此当前版本验收通过。
