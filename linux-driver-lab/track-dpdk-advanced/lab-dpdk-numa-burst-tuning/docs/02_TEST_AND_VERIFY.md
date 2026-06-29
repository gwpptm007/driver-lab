# 02_TEST_AND_VERIFY - 娴嬭瘯鍛戒护涓庢墽琛岃褰?
## 娴嬭瘯璁板綍

```text
records/20260629-212218-numa-burst/
```

## 瀹屾暣鍛戒护

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/lab-dpdk-numa-burst-tuning
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-numa-burst"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_burst_cache_matrix.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 鐢熸垚鏂囦欢

```text
ENV_CHECK.log
BUILD.log
PCAP_GENERATE.log
MATRIX.log
MATRIX.csv
SUMMARY.md
burst_input.pcap
```

## 鍏抽敭杈撳嚭

```text
PASS_BURST_MATRIX
PASS_CACHE_MATRIX
PASS_CPU_RECORD
PASS_LIMITATION_DOC
rows=15
burst_values=5
cache_values=3
```

CSV 琛ㄥご锛?
```text
burst_size,mbuf_cache,rx_packets,rx_bytes,duration_sec,pps,polls,empty_polls
```

## 楠屾敹瑙ｉ噴

| 椤?| 瑙ｉ噴 |
|---|---|
| `PASS_BURST_MATRIX` | 瑕嗙洊 5 涓?burst size |
| `PASS_CACHE_MATRIX` | 瑕嗙洊 3 涓?mempool cache size |
| `PASS_CPU_RECORD` | 璁板綍 CPU/NUMA 淇℃伅 |
| `PASS_LIMITATION_DOC` | 鏄庣‘ pcap PMD 涓嶄唬琛ㄧ湡瀹?NIC pps |
