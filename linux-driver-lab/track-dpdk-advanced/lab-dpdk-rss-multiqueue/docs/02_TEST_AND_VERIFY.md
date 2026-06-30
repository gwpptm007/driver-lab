# 02_TEST_AND_VERIFY - 测试命令与执行记录

## 测试记录

```text
records/20260629-211820-rss-multiqueue/
```

## 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/lab-dpdk-rss-multiqueue
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-rss-multiqueue"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_queue_probe.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 生成文件

```text
ENV_CHECK.log
BUILD.log
PCAP_GENERATE.log
PCAP_QUEUE_PROBE.log
SUMMARY.md
rss_input.pcap
```

## 关键输出

```text
driver_name=net_pcap
max_rx_queues=1
reta_size=0
rss_offloads=0x0
blocked_reason=no_rss_offloads_or_reta
blocked_reason=max_rx_queues_lt_requested requested=2 max_rx_queues=1
```

## 验收解释

| 项 | 结果 | 解释 |
|---|---|---|
| `PASS_BUILD` | PASS | 程序构建成功 |
| `QUEUE_CONFIG` | BLOCKED | pcap PMD 不支持请求的多 RX queue |
| `RSS_QUERY` | BLOCKED | 没有 RETA / RSS offload |
| `QUEUE_TO_CORE_DOC` | PASS | queue-to-core 模型已记录 |

