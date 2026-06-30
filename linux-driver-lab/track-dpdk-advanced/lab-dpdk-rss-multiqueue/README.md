# lab-dpdk-rss-multiqueue

Phase 2：RSS / Multi-Queue capability probe。

## 目标

理解 DPDK 中 RX queue、RSS capability 和 lcore 映射的关系，并在当前测试环境中形成明确 evidence。

## 状态

```text
BLOCKED_PCAP_RSS
```

正式记录：

```text
records/20260629-211820-rss-multiqueue/
```

当前 pcap PMD 能力：

```text
driver_name=net_pcap
max_rx_queues=1
reta_size=0
rss_offloads=0x0
```

## 快速复测

```bash
cd linux-driver-lab/track-dpdk-advanced/lab-dpdk-rss-multiqueue
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-rss-multiqueue"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_queue_probe.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 文档入口

- `docs/01_OVERVIEW.md`
- `docs/02_TEST_AND_VERIFY.md`
- `docs/03_RESULT_ANALYSIS.md`
- `docs/04_DEEP_LEARNING.md`

## 边界

当前 Phase 2 优先做 capability probe，不强行承诺当前 VMware/pcap PMD 能完成真实 RSS 分流。如果 PMD 不支持多队列或 RSS，结果应写成 `BLOCKED_*`，而不是伪造 PASS。

