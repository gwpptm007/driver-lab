# Phase 1 mbuf/mempool Report

## 鐩爣

楠岃瘉 `dpdk-mbuf-inspect` 鑳藉湪 pcap PMD 璺緞涓嬫帴鏀?UDP packet锛屽苟鎵撳嵃鍏抽敭 `rte_mbuf` metadata銆乵empool 閰嶇疆鍜?stats 瀵归綈缁撴灉銆?
## 娴嬭瘯鐜

```text
host: 192.168.65.135
user: wq7
DPDK: 21.11.9
PMD: net_pcap
hugepages: 1024 x 2MB
record: records/20260629-210538-mbuf-mempool
```

娴嬭瘯鍓嶅彂鐜帮細

```text
/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages:0
```

宸插湪娴嬭瘯鏈鸿缃細

```bash
echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
sudo chmod 1777 /dev/hugepages
```

鍘熷洜锛欴PDK EAL 闇€瑕佸垱寤?hugepage backing files锛岄粯璁?`/dev/hugepages` 涓?root-owned 755锛屾櫘閫氱敤鎴疯繍琛屼細瑙﹀彂 permission denied銆?
## 鎵ц鍛戒护

```bash
cd linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive
export RECORD_DIR="$PWD/records/20260629-210538-mbuf-mempool"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_metadata.sh
./scripts/03_collect_report.sh "$RECORD_DIR"
```

## 楠屾敹缁撴灉

| Item | Result |
|------|--------|
| PASS_BUILD | PASS |
| PASS_PCAP_RX | PASS |
| PASS_MBUF_METADATA | PASS |
| PASS_MEMPOOL_CONFIG | PASS |
| PASS_STATS_CONSISTENCY | PASS |

## 鍏抽敭杈撳嚭

```text
software_rx_packets=32
software_rx_bytes=2144
samples_printed=8
ethdev_ipackets=32
ethdev_ibytes=2144
stats_consistency=PASS_STATS_CONSISTENCY
```

mbuf sample 涓凡缁忚瀵熷埌锛?
```text
buf_addr
buf_iova
data_off=128
data_len=67
pkt_len=67
nb_segs=1
ol_flags=0x800000
packet_type=0x0
rss_hash=0x0
refcnt=1
```

## 缁撹

Phase 1 宸茶揪鍒?`PASS_PCAP_METADATA`銆傚綋鍓嶅凡缁忚兘鐢?pcap PMD 鏋勯€犲彲澶嶇幇娴侀噺锛岃瀵?mbuf metadata 鍜?mempool 閰嶇疆锛屽苟瀹屾垚杞欢 RX stats 涓?ethdev RX stats 瀵归綈銆?
鏇磋缁嗙殑鐞嗚В鍜屾祴璇曡繃绋嬭锛?
- `../docs/04_DEEP_LEARNING.md`
- `../docs/02_TEST_AND_VERIFY.md`
- `../docs/03_RESULT_ANALYSIS.md`

## 杈圭晫

鏈樁娈靛彧璇佹槑 pcap PMD RX path 涓嬬殑 mbuf/mempool metadata 瑙傛祴锛屼笉瑕嗙洊锛?
- RSS / multi-queue銆?- NUMA / burst tuning 瀵规瘮銆?- VFIO / IOMMU銆?- 鐪熷疄 NIC RX 璺緞銆?- L3 forwarding / ACL銆?