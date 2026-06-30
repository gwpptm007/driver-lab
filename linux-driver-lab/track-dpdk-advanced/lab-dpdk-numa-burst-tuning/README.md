# lab-dpdk-numa-burst-tuning

Phase 3：burst size / mempool cache / CPU / NUMA 调优方法。

## 目标

建立 DPDK 调优实验的变量控制方法：

- burst size 对比。
- mempool cache size 对比。
- CPU / NUMA / lcore 记录。
- pcap PMD 方法验证边界。

## 状态

```text
PASS_TUNING_METHOD
```

正式记录：

```text
records/20260629-212218-numa-burst/
```

## 快速复测

```bash
cd linux-driver-lab/track-dpdk-advanced/lab-dpdk-numa-burst-tuning
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-numa-burst"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_burst_cache_matrix.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 文档入口

- `docs/01_OVERVIEW.md`
- `docs/02_TEST_AND_VERIFY.md`
- `docs/03_RESULT_ANALYSIS.md`
- `docs/04_DEEP_LEARNING.md`

## 边界

当前使用 pcap PMD 和固定 pcap 文件建立调优方法，不把结果夸大成真实 NIC 性能。真实性能调优需要真实 PMD、RSS、多队列、NUMA 拓扑和稳定压测工具。

