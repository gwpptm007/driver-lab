# lab-dpdk-mbuf-mempool-deep-dive

Phase 1：mbuf / mempool / metadata 深挖。

## 目标

理解 DPDK packet memory model，并用可复现 pcap 流量证明 mbuf metadata 如何贯穿用户态 fastpath。

## 状态

```text
PASS_PCAP_METADATA
```

正式记录：

```text
records/20260629-210538-mbuf-mempool/
```

## 快速复测

```bash
cd linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-mbuf-mempool"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_metadata.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 文档入口

- `docs/01_OVERVIEW.md`
- `docs/02_TEST_AND_VERIFY.md`
- `docs/03_RESULT_ANALYSIS.md`
- `docs/04_DEEP_LEARNING.md`

## 验收项

```text
PASS_BUILD
PASS_PCAP_RX
PASS_MBUF_METADATA
PASS_MEMPOOL_CONFIG
PASS_STATS_CONSISTENCY
```

