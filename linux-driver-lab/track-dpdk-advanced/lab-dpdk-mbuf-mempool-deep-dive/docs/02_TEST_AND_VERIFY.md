# 02_TEST_AND_VERIFY - 娴嬭瘯鍛戒护涓庢墽琛岃褰?
## 娴嬭瘯璁板綍

```text
records/20260629-210538-mbuf-mempool/
```

## 瀹屾暣鍛戒护

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-mbuf-mempool"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_metadata.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 鐢熸垚鏂囦欢

```text
ENV_CHECK.log
BUILD.log
PCAP_GENERATE.log
PCAP_METADATA.log
SUMMARY.md
udp_input.pcap
```

## 鍏抽敭杈撳嚭

```text
software_rx_packets=32
samples_printed=8
PASS_PCAP_RX
PASS_MBUF_METADATA
PASS_MEMPOOL_CONFIG
PASS_STATS_CONSISTENCY
```

mbuf 鏍蜂緥锛?
```text
MBUF_SAMPLE index=0 port=0 mbuf_port=0 data_off=128 data_len=67 pkt_len=67 nb_segs=1 ol_flags=0x800000 rss_hash=0x0 refcnt=1
```

## 楠屾敹瑙ｉ噴

| 椤?| 瑙ｉ噴 |
|---|---|
| `PASS_BUILD` | C 绋嬪簭鏋勫缓鎴愬姛 |
| `PASS_PCAP_RX` | pcap PMD 鏀跺埌浜嗗寘 |
| `PASS_MBUF_METADATA` | 鎴愬姛鎵撳嵃 mbuf metadata |
| `PASS_MEMPOOL_CONFIG` | 鏃ュ織涓褰曚簡 mempool 閰嶇疆 |
| `PASS_STATS_CONSISTENCY` | 杞欢璁℃暟涓?ethdev 璁℃暟涓€鑷?|
