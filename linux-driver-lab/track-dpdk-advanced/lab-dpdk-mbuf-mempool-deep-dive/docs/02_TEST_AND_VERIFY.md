# 02_TEST_AND_VERIFY - 测试命令与执行记录

## 测试记录

```text
records/20260629-210538-mbuf-mempool/
```

## 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-mbuf-mempool"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_metadata.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 生成文件

```text
ENV_CHECK.log
BUILD.log
PCAP_GENERATE.log
PCAP_METADATA.log
SUMMARY.md
udp_input.pcap
```

## 关键输出

```text
software_rx_packets=32
samples_printed=8
PASS_PCAP_RX
PASS_MBUF_METADATA
PASS_MEMPOOL_CONFIG
PASS_STATS_CONSISTENCY
```

mbuf 样例：

```text
MBUF_SAMPLE index=0 port=0 mbuf_port=0 data_off=128 data_len=67 pkt_len=67 nb_segs=1 ol_flags=0x800000 rss_hash=0x0 refcnt=1
```

## 验收解释

| 项 | 解释 |
|---|---|
| `PASS_BUILD` | C 程序构建成功 |
| `PASS_PCAP_RX` | pcap PMD 收到了包 |
| `PASS_MBUF_METADATA` | 成功打印 mbuf metadata |
| `PASS_MEMPOOL_CONFIG` | 日志中记录了 mempool 配置 |
| `PASS_STATS_CONSISTENCY` | 软件计数与 ethdev 计数一致 |

