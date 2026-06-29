# 测试结果分析

正式记录目录：

```text
records/20260629-212218-numa-burst
```

## Summary

```text
PASS_BUILD          PASS
PASS_BURST_MATRIX   PASS
PASS_CACHE_MATRIX   PASS
PASS_CPU_RECORD     PASS
PASS_LIMITATION_DOC PASS
```

## Matrix

```text
rows=15
burst_values=5
cache_values=3
CPU(s)=8
NUMA node(s)=1
```

## 解释原则

如果 pcap PMD 下不同 burst/cache 的 pps 有波动，只能说明当前 synthetic path 下的实验结果。不要写成真实 NIC 调优结论。

推荐结论：

```text
Phase 3 建立了 burst/cache/lcore/NUMA 的记录方法。
pcap PMD 下结果用于方法验证，不用于生产性能判断。
```
