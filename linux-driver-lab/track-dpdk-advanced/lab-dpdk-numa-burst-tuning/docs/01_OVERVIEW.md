# 01_OVERVIEW - NUMA / Burst / Cache tuning method

## 实验问题

这个 lab 用来建立 DPDK 调优实验方法：

```text
burst size 如何影响一次 poll 取到的 packet 数量和 pps？
mempool cache size 如何作为调优变量记录？
lcore / CPU / NUMA 环境如何进入 records？
pcap PMD 下哪些结果只能作为方法验证，不能作为真实 NIC 性能结论？
```

## 实验路径

```text
fixed pcap input
  -> dpdk-burst-cache-probe
  -> burst size matrix
  -> mempool cache matrix
  -> MATRIX.csv
  -> SUMMARY.md
```

## 验收项

```text
PASS_BUILD
PASS_BURST_MATRIX
PASS_CACHE_MATRIX
PASS_CPU_RECORD
PASS_LIMITATION_DOC
```

## 文档入口

- `02_TEST_AND_VERIFY.md`：逐步测试命令与执行记录。
- `03_RESULT_ANALYSIS.md`：测试结果分析。
- `04_DEEP_LEARNING.md`：NUMA/burst/cache 原理、变量控制和矩阵方法。

## 当前边界

当前使用 pcap PMD 和固定 pcap 文件建立调优方法，不把结果夸大成真实 NIC 性能。真实性能调优需要真实 PMD、RSS、多队列、NUMA 拓扑和稳定压测工具。

