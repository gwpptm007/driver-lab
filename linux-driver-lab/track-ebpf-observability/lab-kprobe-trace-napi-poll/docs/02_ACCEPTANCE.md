# 02_ACCEPTANCE

## 最低通过标准

```text
ENV_CHECK.txt 存在
NAPI_PROBE_POINTS.txt 存在
NAPI_POLL_KPROBE.log 存在且无 bpftrace 语法错误
SOFTIRQ_NAPI_CORRELATION.log 存在
COLLECT_STATS.txt 存在
REVIEW_BUNDLE.md 存在
```

## 推荐通过标准

```text
napi_poll calls > 0
napi_poll return value 统计存在
NET_RX softirq events > 0
至少发现一个 driver poll 相关符号
```

## 允许的 WARN

下列情况允许记为 WARN，不直接失败：

```text
kretprobe:napi_poll 不可附加
特定驱动 poll 函数没有符号
notrace 函数不可 kprobe
测试期间目标网卡无流量导致计数为 0
```

## 不通过

```text
bpftrace 不存在
所有 bpftrace 脚本都语法失败
records 没有生成
review bundle 没有生成
```
