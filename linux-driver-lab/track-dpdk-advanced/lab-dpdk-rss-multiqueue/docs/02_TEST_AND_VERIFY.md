# 02_TEST_AND_VERIFY - 娴嬭瘯鍛戒护涓庢墽琛岃褰?
## 娴嬭瘯璁板綍

```text
records/20260629-211820-rss-multiqueue/
```

## 瀹屾暣鍛戒护

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/lab-dpdk-rss-multiqueue
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-rss-multiqueue"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_queue_probe.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 鐢熸垚鏂囦欢

```text
ENV_CHECK.log
BUILD.log
PCAP_GENERATE.log
PCAP_QUEUE_PROBE.log
SUMMARY.md
rss_input.pcap
```

## 鍏抽敭杈撳嚭

```text
driver_name=net_pcap
max_rx_queues=1
reta_size=0
rss_offloads=0x0
blocked_reason=no_rss_offloads_or_reta
blocked_reason=max_rx_queues_lt_requested requested=2 max_rx_queues=1
```

## 楠屾敹瑙ｉ噴

| 椤?| 缁撴灉 | 瑙ｉ噴 |
|---|---|---|
| `PASS_BUILD` | PASS | 绋嬪簭鏋勫缓鎴愬姛 |
| `QUEUE_CONFIG` | BLOCKED | pcap PMD 涓嶆敮鎸佽姹傜殑澶?RX queue |
| `RSS_QUERY` | BLOCKED | 娌℃湁 RETA / RSS offload |
| `QUEUE_TO_CORE_DOC` | PASS | queue-to-core 妯″瀷宸茶褰?|
