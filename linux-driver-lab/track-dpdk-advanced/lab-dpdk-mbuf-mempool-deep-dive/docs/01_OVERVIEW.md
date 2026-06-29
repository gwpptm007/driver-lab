# lab-dpdk-mbuf-mempool-deep-dive Overview

## 瀹為獙闂

```text
mbuf 閲屼繚瀛樹簡鍝簺 packet metadata锛?mempool cache 濡備綍褰卞搷 packet buffer 鍒嗛厤锛?rx_burst 鏀跺埌鐨?packet 濡備綍杩涘叆 mbuf inspect 璺緞锛?杞欢 stats 鍜?ethdev stats 濡備綍瀵归綈锛?```

## 棰勬湡鐩綍

瀹炵幇闃舵寤鸿浣跨敤锛?
```text
lab-dpdk-mbuf-mempool-deep-dive/
鈹溾攢鈹€ README.md
鈹溾攢鈹€ docs/
鈹溾攢鈹€ app/
鈹?  鈹溾攢鈹€ main.c
鈹?  鈹斺攢鈹€ meson.build
鈹溾攢鈹€ scripts/
鈹?  鈹溾攢鈹€ 00_check_env.sh
鈹?  鈹溾攢鈹€ 01_build.sh
鈹?  鈹溾攢鈹€ 02_run_pcap_metadata.sh
鈹?  鈹斺攢鈹€ 03_collect_report.sh
鈹溾攢鈹€ records/
鈹斺攢鈹€ reports/
```

## 寤鸿瑙傛祴瀛楁

```text
mbuf->buf_addr
mbuf->buf_iova
mbuf->data_off
mbuf->data_len
mbuf->pkt_len
mbuf->nb_segs
mbuf->port
mbuf->ol_flags
```

## 楠屾敹鑽夋

```text
PASS_BUILD
PASS_PCAP_RX
PASS_MBUF_METADATA
PASS_MEMPOOL_CONFIG
PASS_STATS_CONSISTENCY
```

## 鏂囨。鍏ュ彛

- `04_DEEP_LEARNING.md`锛歮buf/mempool 鍘熺悊銆丮ermaid 鍥惧拰鐢熷懡鍛ㄦ湡鎷嗚В銆?- `02_TEST_AND_VERIFY.md`锛氶€愭娴嬭瘯鍛戒护涓庢墽琛岃褰曘€?- `03_RESULT_ANALYSIS.md`锛氭祴璇曠粨鏋滃垎鏋愩€?
## 褰撳墠杈圭晫

- 涓嶅仛鐪熷疄 NIC 鎬ц兘璋冧紭銆?- 涓嶅仛 RSS 澶氶槦鍒椼€?- 涓嶅仛 VFIO/IOMMU 鐜鏀归€犮€?- 鍙仛鐒?mbuf/mempool 涓?packet metadata銆?