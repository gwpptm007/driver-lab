# lab-dpdk-mbuf-mempool-deep-dive

> track-dpdk-advanced Phase 1锛歮buf / mempool / metadata 娣辨寲銆?
## 鐩爣

鐞嗚В DPDK packet memory model锛屽苟鐢ㄥ彲澶嶇幇娴侀噺璇佹槑 mbuf metadata 濡備綍璐┛鐢ㄦ埛鎬?fastpath锛?
```text
pcap PMD
  -> rte_eth_rx_burst()
  -> rte_mbuf
  -> inspect metadata
  -> mempool state
  -> ethdev/software stats compare
```

## 璁″垝鏂囦欢

- `docs/01_OVERVIEW.md`
- `docs/04_DEEP_LEARNING.md`
- `docs/02_TEST_AND_VERIFY.md`
- `docs/03_RESULT_ANALYSIS.md`
- `../docs/01_TRACK_OVERVIEW.md`
- `../docs/04_ARCHITECTURE_PRINCIPLES.md`
- `../docs/02_ACCEPTANCE.md`

## 褰撳墠鐘舵€?
```text
PASS_PCAP_METADATA
```

褰撳墠宸茶惤鍦版渶灏忓疄鐜帮細

```text
app/dpdk-mbuf-inspect
scripts/00_check_env.sh
scripts/01_build.sh
scripts/02_run_pcap_metadata.sh
scripts/03_collect_report.sh
tools/gen_udp_pcap.py
```

宸插湪娴嬭瘯鏈?`192.168.65.135` 瀹屾垚 pcap PMD 璺緞楠岃瘉锛?
```text
record: records/20260629-210538-mbuf-mempool
PASS_BUILD
PASS_PCAP_RX
PASS_MBUF_METADATA
PASS_MEMPOOL_CONFIG
PASS_STATS_CONSISTENCY
```

澶嶆祴鍛戒护锛?
```bash
cd linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive
chmod +x scripts/*.sh tools/*.py
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_metadata.sh
./scripts/03_collect_report.sh "$(find records -maxdepth 1 -type d -name '*-mbuf-mempool' | sort | tail -1)"
```

楠屾敹椤癸細

```text
PASS_BUILD
PASS_PCAP_RX
PASS_MBUF_METADATA
PASS_MEMPOOL_CONFIG
PASS_STATS_CONSISTENCY or CHECK_STATS_CONSISTENCY
```
