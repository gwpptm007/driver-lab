# project-dpdk-l3-forwarder-lite

DPDK Advanced Phase 5锛氫竴涓皬鑰屽畬鏁寸殑 L3 forwarding / ACL / per-rule stats 椤圭洰銆?
## 鐩爣

鍦ㄥ綋鍓?VMware 娴嬭瘯鐜閲岀户缁娇鐢ㄥ彲澶嶇幇鐨?pcap PMD锛?
```text
pcap PMD input -> IPv4/UDP parse -> ACL drop or L3 forward -> net_null PMD output
```

杩欎釜椤圭洰涓嶅０绉版槸鐪熷疄 NIC 绾块€熻浆鍙戯紝閲嶇偣鏄妸 L3 鏁版嵁闈㈠伐绋嬮鏋惰娓呮锛?
- route table / longest-prefix 鎬濊矾
- ACL drop rule
- per-rule stats
- pcap evidence
- DPDK port RX/TX 鐢熷懡鍛ㄦ湡

## 蹇€熸墽琛?
```bash
cd linux-driver-lab/track-dpdk-advanced/project-dpdk-l3-forwarder-lite
chmod +x scripts/*.sh tools/*.py
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_l3_forward.sh
./scripts/03_collect_report.sh
```

## 楠屾敹椤?
```text
PASS_BUILD
PASS_ROUTE_CONFIG
PASS_L3_FORWARD
PASS_ACL_DROP
PASS_PER_RULE_STATS
PASS_PCAP_EVIDENCE
```

## 娣卞害鏂囨。

寤鸿闃呰椤哄簭锛?
```text
docs/01_OVERVIEW.md
docs/04_DEEP_LEARNING.md
docs/02_TEST_AND_VERIFY.md
docs/03_RESULT_ANALYSIS.md
```

鍏朵腑锛?
- `04_DEEP_LEARNING.md`锛氬師鐞嗗浘銆佷唬鐮佽矾寰勩€乵buf 鐢熷懡鍛ㄦ湡銆丄CL/route/stats 璁捐銆?- `02_TEST_AND_VERIFY.md`锛氭祴璇曟満鍛戒护銆佸疄闄?DPDK 鍛戒护銆佹棩蹇楁枃浠躲€佸叧閿緭鍑哄拰楠屾敹瑙ｉ噴銆?