# 02_ACCEPTANCE

## PASS_ENV

```text
bpftrace 可执行
tracefs/debugfs 可访问
目标网卡存在
```

## PASS_PROBE_LIST

```text
PROBE_POINTS.txt 中能列出 net/irq tracepoints。
```

## PASS_TRACEPOINT_SMOKE

```text
RX_TRACEPOINT.log / TX_TRACEPOINT.log / SOFTIRQ_TRACEPOINT.log 至少一个正常生成，且没有 BPFTRACE_RC=非0。
```

## PASS_TRAFFIC_OBSERVED

```text
tracepoint 日志里出现非 0 count。
```

## KPROBE_OPTIONAL

```text
KPROBE_OPTIONAL.log 有记录即可。
失败原因如果是 BTF/notrace/symbol 不可用，记 NOTE，不阻塞本 lab。
```

## PASS_REVIEW

```text
REVIEW_BUNDLE.md 生成，并明确记录 PASS/NOTE/BACKLOG。
```
