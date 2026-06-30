# 02_TEST_AND_VERIFY - 测试命令与执行记录

## 测试记录

```text
records/20260629-212218-numa-burst/
```

## 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/lab-dpdk-numa-burst-tuning
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-numa-burst"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_burst_cache_matrix.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 生成文件

```text
ENV_CHECK.log
BUILD.log
PCAP_GENERATE.log
MATRIX.log
MATRIX.csv
SUMMARY.md
burst_input.pcap
```

## 关键输出

```text
PASS_BURST_MATRIX
PASS_CACHE_MATRIX
PASS_CPU_RECORD
PASS_LIMITATION_DOC
rows=15
burst_values=5
cache_values=3
```

CSV 表头：

```text
burst_size,mbuf_cache,rx_packets,rx_bytes,duration_sec,pps,polls,empty_polls
```

## 验收解释

| 项 | 解释 |
|---|---|
| `PASS_BURST_MATRIX` | 覆盖 5 个 burst size |
| `PASS_CACHE_MATRIX` | 覆盖 3 个 mempool cache size |
| `PASS_CPU_RECORD` | 记录 CPU/NUMA 信息 |
| `PASS_LIMITATION_DOC` | 明确 pcap PMD 不代表真实 NIC pps |

