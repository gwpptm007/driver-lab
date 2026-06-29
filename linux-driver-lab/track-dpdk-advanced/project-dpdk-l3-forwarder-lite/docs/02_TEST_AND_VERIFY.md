# 02_TEST_AND_VERIFY - 娴嬭瘯鍛戒护涓庢墽琛岃褰?
> 杩欑瘒璁板綍 Phase 5 鍦?`192.168.65.135` 娴嬭瘯鏈轰笂鐨勫疄闄呮墽琛屽懡浠ゃ€佹棩蹇椾綅缃€佸叧閿緭鍑哄拰楠屾敹鍒ゆ柇銆?
## 1. 娴嬭瘯鏈虹幆澧?
```text
Host: 192.168.65.135
User: wq7
Remote repo: /home/wq7/workspace/driver-lab
Project: linux-driver-lab/track-dpdk-advanced/project-dpdk-l3-forwarder-lite
Record: records/20260629-213104-l3-forwarder/
```

鐜鏃ュ織锛?
```text
records/20260629-213104-l3-forwarder/ENV_CHECK.log
```

鍏抽敭杈撳嚭锛?
```text
DPDK version: 21.11.9
Python: 3.10.12
2MB hugepages: 1024
CPU lcores detected by EAL: 8
NUMA nodes detected by EAL: 1
```

## 2. 瀹屾暣鎵ц鍛戒护

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/project-dpdk-l3-forwarder-lite
chmod +x scripts/*.sh tools/*.py
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-l3-forwarder"
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_l3_forward.sh
./scripts/03_collect_report.sh
cat "$RECORD_DIR/SUMMARY.md"
```

鏈瀹為檯璁板綍鐩綍锛?
```text
records/20260629-213104-l3-forwarder/
```

## 3. Step 0: 鐜妫€鏌?
鍛戒护锛?
```bash
./scripts/00_check_env.sh
```

鐢熸垚锛?
```text
ENV_CHECK.log
```

鐢ㄩ€旓細

- 璁板綍 kernel銆?- 璁板綍 DPDK 鐗堟湰銆?- 璁板綍 Python 鐗堟湰銆?- 璁板綍 hugepage 鐘舵€併€?
## 4. Step 1: 鏋勫缓绋嬪簭

鍛戒护锛?
```bash
./scripts/01_build.sh
```

鐢熸垚锛?
```text
BUILD.log
```

鍏抽敭杈撳嚭锛?
```text
cc -O2 -g -Wall -Wextra ... main.c -o build/dpdk-l3-forwarder-lite ...
build/dpdk-l3-forwarder-lite: ELF 64-bit LSB pie executable
```

楠屾敹锛?
```text
PASS_BUILD
```

## 5. Step 2: 鐢熸垚 pcap

鍛戒护鐢辫剼鏈唴閮ㄦ墽琛岋細

```bash
python3 tools/gen_l3_pcap.py "$PCAP_FILE" "$L3_PCAP_COUNT"
```

鐢熸垚锛?
```text
l3_input.pcap
PCAP_GENERATE.log
```

鍏抽敭杈撳嚭锛?
```text
Generated 48 mixed IPv4/UDP packets -> .../l3_input.pcap
```

娴侀噺姣斾緥锛?
```text
12 packets: 10.20.0.77:9999 -> ACL drop
12 packets: 10.99.0.77:9000 -> route miss
24 packets: 10.20.0.77:9000 -> forward
```

## 6. Step 3: 杩愯 L3 forwarder

鑴氭湰锛?
```bash
./scripts/02_run_pcap_l3_forward.sh
```

瀹為檯 DPDK 鍛戒护锛?
```bash
app/build/dpdk-l3-forwarder-lite \
  -l 0-1 -n 4 --no-pci \
  --file-prefix dpdk_l3_forwarder_lite_<timestamp> \
  --vdev "net_pcap0,rx_pcap=records/20260629-213104-l3-forwarder/l3_input.pcap" \
  --vdev "net_null1" \
  -- \
  --burst-size 16 \
  --mbuf-cache 250 \
  --max-idle-polls 100000
```

涓轰粈涔堢敤 `--no-pci`锛?
```text
鏈祴璇曞彧楠岃瘉杞欢鏁版嵁闈紝閬垮厤鎵弿鎴栨搷浣滅湡瀹炵綉鍗°€?```

涓轰粈涔堢敤 `net_null1`锛?
```text
瀹冧綔涓?TX sink锛岃兘楠岃瘉 tx_burst 鎴愬姛锛屼笉闇€瑕佺湡瀹為摼璺绔€?```

## 7. Step 4: 杩愯缁撴灉

鏃ュ織锛?
```text
L3_FORWARD.log
```

鍏抽敭杈撳嚭锛?
```text
CONFIG in_port=0 out_port=1 burst=16 nb_mbuf=8192 mbuf_cache=250
ROUTE[0] prefix=10.20.0.0/24 out_port=1
ACL[0] action=drop udp_dst_port=9999
RESULT rx_packets=48 rx_bytes=3264 forwarded_packets=24 forwarded_bytes=1608 acl_drops=12 route_miss_drops=12 non_ipv4_drops=0 parse_drops=0 tx_failed=0 polls=100003 empty_polls=100000
ROUTE_STATS[0] hits=24 bytes=1608
ACL_STATS[0] drops=12 bytes=816
```

瑙ｉ噴锛?
| 瀛楁 | 鍊?| 鍚箟 |
|---|---:|---|
| `rx_packets` | 48 | pcap 涓墍鏈夊寘閮借 RX 鍒?|
| `forwarded_packets` | 24 | route hit 涓?TX 鎴愬姛 |
| `acl_drops` | 12 | 鍛戒腑 UDP dst port 9999 |
| `route_miss_drops` | 12 | 涓嶅懡涓?`10.20.0.0/24` |
| `tx_failed` | 0 | net_null TX 娌℃湁澶辫触 |
| `ROUTE_STATS[0].hits` | 24 | per-route stats 涓?forward 瀵归綈 |
| `ACL_STATS[0].drops` | 12 | per-ACL stats 涓?drop 瀵归綈 |

## 8. Step 5: 姹囨€婚獙鏀?
鍛戒护锛?
```bash
./scripts/03_collect_report.sh
cat records/20260629-213104-l3-forwarder/SUMMARY.md
```

楠屾敹缁撴灉锛?
```text
PASS_BUILD
PASS_ROUTE_CONFIG
PASS_L3_FORWARD
PASS_ACL_DROP
PASS_PER_RULE_STATS
PASS_PCAP_EVIDENCE
```

瀵瑰簲鏂囦欢锛?
```text
records/20260629-213104-l3-forwarder/SUMMARY.md
```

## 9. 鏁呴殰鎺掓煡

### `need at least 2 DPDK ports`

妫€鏌?`--vdev` 鏄惁鍚屾椂鍖呭惈锛?
```text
net_pcap0
net_null1
```

### `rx_packets=0`

妫€鏌ワ細

- pcap 鏂囦欢鏄惁瀛樺湪銆?- `rx_pcap=` 璺緞鏄惁姝ｇ‘銆?- `tools/gen_l3_pcap.py` 鏄惁鎴愬姛鎵ц銆?
### `forwarded_packets=0`

妫€鏌ワ細

- route 鏄惁鎵撳嵃锛歚ROUTE[0] prefix=10.20.0.0/24 out_port=1`
- pcap 涓槸鍚︽湁鐩殑 IP `10.20.0.77`
- ACL 鏄惁鎶婃祦閲忔彁鍓?drop銆?
### `tx_failed>0`

妫€鏌ワ細

- `net_null1` 鏄惁鍒涘缓鎴愬姛銆?- TX queue 鏄惁鍒濆鍖栨垚鍔熴€?- app 鏄惁浣跨敤浜嗘纭殑 `out_port=1`銆?