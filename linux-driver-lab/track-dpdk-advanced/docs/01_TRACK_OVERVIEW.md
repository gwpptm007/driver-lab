# Track Overview

`track-dpdk-advanced` 鐨勫畾浣嶆槸 DPDK 杩涢樁璁粌涓荤嚎銆?
瀹冧笉鏄噸澶嶅熀纭€ track 鐨?hugepage/EAL/testpmd锛岃€屾槸缁х画琛ラ綈锛?
- mbuf/mempool 鏁版嵁缁撴瀯鐞嗚В銆?- RSS/queue/core 鐨勮兘鍔涙帰娴嬪拰杈圭晫璁板綍銆?- burst/cache/NUMA 鐨勮皟浼樺疄楠屾柟娉曘€?- UIO/VFIO/IOMMU/MSI-X 鐨勯儴缃茶竟鐣屻€?- 涓€涓皬鍨?L3 forwarding / ACL / stats 鏁版嵁闈㈤」鐩€?
濡傛灉瑕佺湅鏁翠綋鍘熺悊鍜?UML/Mermaid 鍥撅紝鍏堣锛?
```text
docs/04_ARCHITECTURE_PRINCIPLES.md
```

## 鏈€缁堢姸鎬?
```text
COMPLETED_WITH_BOUNDARIES
```

## 闃舵璁″垝涓庣粨鏋?
| Phase | 鐩綍 | 鐘舵€?|
|---|---|---|
| 1 | `lab-dpdk-mbuf-mempool-deep-dive` | `PASS_PCAP_METADATA` |
| 2 | `lab-dpdk-rss-multiqueue` | `BLOCKED_PCAP_RSS` |
| 3 | `lab-dpdk-numa-burst-tuning` | `PASS_TUNING_METHOD` |
| 4 | `lab-dpdk-vfio-iommu-boundary` | `PASS_VFIO_IOMMU_BOUNDARY` |
| 5 | `project-dpdk-l3-forwarder-lite` | `PASS_L3_FORWARDER_LITE` |
| 6 | `project-dpdk-advanced-summary` | `PASS_ADVANCED_REPORT` |

## DPDK advanced model

```text
PMD / ethdev port
  -> RX queue
  -> rte_eth_rx_burst()
  -> rte_mbuf
  -> parse / classify / tune / inspect
  -> rte_eth_tx_burst() or rte_pktmbuf_free()
```

褰撳墠椤圭洰鎸夎繖鏉¤矾寰勬媶瑙ｏ細

- Phase 1 鐪?mbuf/mempool銆?- Phase 2 鐪?queue/RSS capability銆?- Phase 3 鐪?burst/cache/NUMA 鍙橀噺鎺у埗銆?- Phase 4 鐪?PMD binding銆乂FIO/IOMMU/MSI-X 閮ㄧ讲杈圭晫銆?- Phase 5 鎶?parse/classify/action/TX 鍋氭垚 L3 forwarder lite銆?
## 涓轰粈涔堜繚鐣?boundary

褰撳墠娴嬭瘯鐜鏄?VMware锛屽苟涓昏浣跨敤 pcap PMD / net_null PMD 鍋氬彲澶嶇幇楠岃瘉銆?
鍥犳锛?
- 杞欢鏁版嵁闈㈤€昏緫鍙互楠岃瘉銆?- pcap PMD 鐨?RSS 鑳藉姏闄愬埗蹇呴』璁板綍涓?boundary銆?- 娌℃湁 IOMMU group 鏃讹紝VFIO 鍙兘鍋?prerequisites checklist锛屼笉鑳藉绉扮湡瀹為獙璇佸畬鎴愩€?