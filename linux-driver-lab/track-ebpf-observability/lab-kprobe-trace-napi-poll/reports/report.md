# lab-kprobe-trace-napi-poll 报告

## 最终结论

当前版本判定为：

```text
PASS_NAPI_OBSERVE
```

这次通过不是因为固定 `napi_poll` 成功，而是因为脚本修正后能自动选择当前内核可观测的 `__napi_poll`。

## 实测环境

```text
测试时间: 2026-05-18 21:46:41 +08:00
测试机: VMware Ubuntu
内核: 6.8.0-111-generic
测试接口: ens33
驱动: e1000
bpftrace: v0.14.0
记录目录: records/20260518-214641-kprobe-trace-napi-poll/
```

## 关键证据

```text
NAPI kprobe:
  selected probe: kprobe:__napi_poll
  @napi_poll_calls[kprobe:__napi_poll]: 37

NAPI kretprobe:
  selected probe: kretprobe:__napi_poll
  @napi_poll_returns[kretprobe:__napi_poll, 0]: 11
  @napi_poll_returns[kretprobe:__napi_poll, 1]: 27

softirq correlation:
  @net_rx_softirq_entry: 4
  @net_rx_softirq_exit: 4
  @napi_poll_calls[kprobe:__napi_poll]: 4

review bundle:
  PASS_ENV: YES
  PASS_PROBE_LIST: YES
  PASS_NAPI_KPROBE: YES
  PASS_NAPI_RETPROBE: YES
  PASS_SOFTIRQ_CORRELATION: YES
  TRAFFIC_OR_EVENTS_OBSERVED: YES
```

## 修正点

旧版本的问题是固定挂 `napi_poll`，而测试机内核实际提示该符号不可 trace，导致：

```text
ERROR: Error attaching probe: 'kprobe:napi_poll'
RC=255
```

更严重的是旧的 `07_make_review_bundle.sh` 没有把 `RC=255` 和 attach failure 识别为失败，曾经误判为 PASS。

当前版本已经修正：

```text
1. 动态选择 napi_poll / __napi_poll / poll_one_napi / napi_threaded_poll。
2. softirq tracepoint 和 NAPI kprobe 解耦。
3. review bundle 明确识别 NO_ATTACH_FAILED / WARN_NOT_TRACEABLE。
4. 每轮动态生成的 .bt 脚本保存在 records/<本轮目录>/ 中，方便复盘。
```

## 下一步

本 lab 已收尾。按照 `track-ebpf-observability/README.md`，下一站是 `lab-tracepoint-skb-path`：把本次掌握的 NAPI/softirq 观测继续连接到 skb 层 tracepoint，学习如何更稳定地观察 RX/TX 路径上的 skb。
