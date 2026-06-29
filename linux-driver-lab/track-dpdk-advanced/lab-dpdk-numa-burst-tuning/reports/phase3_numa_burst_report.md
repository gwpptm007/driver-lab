# Phase 3 NUMA / burst tuning Report

## 目标

建立 burst size、mempool cache size、lcore/CPU/NUMA 的记录方法，并输出可复查矩阵。

## 测试环境

```text
host: 192.168.65.135
DPDK: 21.11.9
PMD: net_pcap
record: records/20260629-212218-numa-burst
```

CPU:

```text
CPU(s): 8
NUMA node(s): 1
NUMA node0 CPU(s): 0-7
```

## 验收结果

| Item | Result |
|------|--------|
| PASS_BUILD | PASS |
| PASS_BURST_MATRIX | PASS |
| PASS_CACHE_MATRIX | PASS |
| PASS_CPU_RECORD | PASS |
| PASS_LIMITATION_DOC | PASS |

## 矩阵

```text
burst size: 1, 4, 16, 32, 64
mempool cache: 0, 64, 250
rows: 15
pcap packets per run: 4096
```

## 结论

Phase 3 已达到 `PASS_TUNING_METHOD`。当前已经形成可复测的 burst/cache/lcore/NUMA 记录方法。

## 边界

当前使用 pcap PMD，不代表真实 NIC 性能。矩阵结果用于证明调优方法和记录格式，不用于声称生产 pps 或真实 NUMA 优化效果。
