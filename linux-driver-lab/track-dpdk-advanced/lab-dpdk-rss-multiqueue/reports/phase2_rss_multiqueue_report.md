# Phase 2 RSS / multiqueue Report

## 目标

验证当前 PMD/环境是否支持 RSS 和多 RX queue，并形成 queue-to-core mapping 说明。

## 测试环境

```text
host: 192.168.65.135
DPDK: 21.11.9
PMD: net_pcap
record: records/20260629-211820-rss-multiqueue
```

## 执行命令

```bash
cd linux-driver-lab/track-dpdk-advanced/lab-dpdk-rss-multiqueue
export RECORD_DIR="$PWD/records/20260629-211820-rss-multiqueue"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_queue_probe.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
```

## 验收结果

| Item | Result |
|------|--------|
| PASS_BUILD | PASS |
| QUEUE_CONFIG | BLOCKED_QUEUE_CONFIG |
| RSS_QUERY | BLOCKED_RSS |
| QUEUE_TO_CORE_DOC | PASS_QUEUE_TO_CORE_DOC |

## 关键证据

```text
driver_name=net_pcap
requested_rx_queues=2
max_rx_queues=1
reta_size=0
rss_offloads=0x0
```

blocked reasons:

```text
blocked_reason=no_rss_offloads_or_reta
blocked_reason=max_rx_queues_lt_requested requested=2 max_rx_queues=1
```

queue-to-core mapping:

```text
queue_map rxq=0 lcore=1
queue_map rxq=1 lcore=2
```

## 结论

Phase 2 在 pcap PMD 下完成 RSS/multiqueue capability probe。当前 `net_pcap` 可作为稳定流量输入 PMD，但不支持真实 RSS，也不支持请求的 2 RX queue 配置。

因此本阶段收口为：

```text
BLOCKED_PCAP_RSS
```

这不是失败，而是环境能力边界。后续如需 `PASS_QUEUE_CONFIG` 和 `PASS_RSS_QUERY`，需要切换到支持多队列/RSS 的真实 PMD。
