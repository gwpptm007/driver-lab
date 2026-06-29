# lab-dpdk-rss-multiqueue

> track-dpdk-advanced Phase 2锛歊SS / multi-queue / queue-to-core mapping 鑳藉姏鎺㈡祴銆?
## 鐩爣

鐞嗚В DPDK 涓?RX queue銆丷SS capability 鍜?lcore 鏄犲皠鐨勫叧绯伙紝骞跺湪褰撳墠娴嬭瘯鐜涓舰鎴愭槑纭?evidence锛?
```text
PMD capability query
  -> max_rx_queues / max_tx_queues
  -> rss offload flags / reta_size
  -> attempt multi-queue configure
  -> PASS or BLOCKED evidence
  -> queue-to-core mapping doc
```

## 褰撳墠鐘舵€?
```text
BLOCKED_PCAP_RSS
```

## 褰撳墠瀹炵幇

```text
app/dpdk-rss-queue-probe
scripts/00_check_env.sh
scripts/01_build.sh
scripts/02_run_pcap_queue_probe.sh
scripts/03_collect_report.sh
tools/gen_udp_pcap.py
```

## 楠屾敹椤?
```text
PASS_QUEUE_CONFIG or BLOCKED_QUEUE_CONFIG
PASS_RSS_QUERY or BLOCKED_RSS
PASS_QUEUE_TO_CORE_DOC
```

宸插湪娴嬭瘯鏈?`192.168.65.135` 瀹屾垚 pcap PMD capability probe锛?
```text
record: records/20260629-211820-rss-multiqueue
PASS_BUILD
BLOCKED_QUEUE_CONFIG
BLOCKED_RSS
PASS_QUEUE_TO_CORE_DOC
```

杈圭晫缁撹锛?
```text
net_pcap max_rx_queues=1
net_pcap rss_offloads=0x0
net_pcap reta_size=0
```

## 澶嶆祴鍛戒护

```bash
cd linux-driver-lab/track-dpdk-advanced/lab-dpdk-rss-multiqueue
chmod +x scripts/*.sh tools/*.py
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-rss-multiqueue"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_queue_probe.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 鏂囨。鍏ュ彛

- `docs/01_OVERVIEW.md`
- `docs/04_DEEP_LEARNING.md`
- `docs/02_TEST_AND_VERIFY.md`
- `docs/03_RESULT_ANALYSIS.md`

## 杈圭晫

褰撳墠 Phase 2 浼樺厛鍋?capability probe锛屼笉寮鸿鎵胯褰撳墠 VMware/pcap PMD 鑳藉畬鎴愮湡瀹?RSS 鍒嗘祦銆傚鏋?PMD 涓嶆敮鎸佸闃熷垪鎴?RSS锛岀粨鏋滃簲鍐欐垚 `BLOCKED_*`锛岃€屼笉鏄吉閫?PASS銆?