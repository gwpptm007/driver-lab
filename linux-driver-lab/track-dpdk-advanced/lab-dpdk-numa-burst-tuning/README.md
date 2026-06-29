# lab-dpdk-numa-burst-tuning

> track-dpdk-advanced Phase 3锛歜urst size / mempool cache / lcore 鍙傛暟璁板綍鏂规硶銆?
## 鐩爣

寤虹珛 DPDK 璋冧紭瀹為獙鐨勫彉閲忔帶鍒舵柟娉曪細

```text
burst size matrix
  -> mempool cache matrix
  -> lcore record
  -> pps / rx_packets / duration
  -> limitation doc
```

## 褰撳墠鐘舵€?
```text
PASS_TUNING_METHOD
```

宸插湪娴嬭瘯鏈?`192.168.65.135` 瀹屾垚鐭╅樀娴嬭瘯锛?
```text
record: records/20260629-212218-numa-burst
PASS_BUILD
PASS_BURST_MATRIX
PASS_CACHE_MATRIX
PASS_CPU_RECORD
PASS_LIMITATION_DOC
```

## 楠屾敹椤?
```text
PASS_BURST_MATRIX
PASS_CACHE_MATRIX
PASS_CPU_RECORD
PASS_LIMITATION_DOC
```

## 澶嶆祴鍛戒护

```bash
cd linux-driver-lab/track-dpdk-advanced/lab-dpdk-numa-burst-tuning
chmod +x scripts/*.sh tools/*.py
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-numa-burst"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_burst_cache_matrix.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 鏂囨。鍏ュ彛

- `docs/01_OVERVIEW.md`
- `docs/04_DEEP_LEARNING.md`
- `docs/02_TEST_AND_VERIFY.md`
- `docs/03_RESULT_ANALYSIS.md`

## 杈圭晫

褰撳墠浣跨敤 pcap PMD 鍜屽浐瀹?pcap 鏂囦欢寤虹珛璋冧紭鏂规硶锛屼笉鎶婄粨鏋滃じ澶ф垚鐪熷疄 NIC 鎬ц兘銆傜湡瀹炲悶鍚愯皟浼橀渶瑕佺湡瀹?PMD銆丷SS銆佸闃熷垪銆丯UMA 鎷撴墤鍜岀ǔ瀹氬帇娴嬪伐鍏枫€?