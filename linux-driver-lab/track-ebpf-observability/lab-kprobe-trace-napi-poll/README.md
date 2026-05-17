# lab-kprobe-trace-napi-poll

> eBPF observability Phase 2：专门围绕 NAPI poll 做 kprobe/kretprobe 观测。

## 定位

上一站 `lab-bpftrace-netdev-observe` 已经验证 tracepoint-first 的 RX/TX/softirq 观测路径。本 lab 更聚焦 NAPI：

```text
softirq NET_RX
    ↓
napi_poll()
    ↓
驱动 poll 函数，例如 vmxnet3_poll / napi_gro_receive / napi_complete_done
    ↓
CPU 分布、调用频率、返回值、预算关系
```

## 为什么单独做这一站

NAPI 是 Linux 网络 RX 路径的核心调度层。驱动面试或内核网络排障时，经常要解释：

```text
中断为什么会转成 poll？
softirq 和 napi_poll 是什么关系？
NAPI poll 在哪个 CPU 上跑？
一次 poll 处理多少包？
驱动 poll 函数是否真的被调用？
```

## 测试顺序

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

如果目标网卡没有流量，可以先用：

```bash
./scripts/08_traffic_hint.sh
```

## 预期结果

最低通过：

```text
PASS_ENV=YES
PASS_PROBE_LIST=YES
PASS_NAPI_KPROBE=YES 或 NAPI_KPROBE_OPTIONAL_WARN
PASS_SOFTIRQ_CORRELATION=YES
REVIEW_BUNDLE.md 生成
```

如果测试机内核/安全策略不允许某些 kprobe，本 lab 不直接判失败，而是记录为 optional/fallback。工程上要学会区分：

```text
观测点不存在/不可附加 != 网络路径不存在
```
