# lab-dpdk-rss-multiqueue Overview

## 瀹為獙闂

```text
褰撳墠 PMD 鏀寔澶氬皯 RX/TX queue锛?RSS capability 鏄惁鍙煡璇紵
RETA / hash_key / rss_hf 鏄惁瀛樺湪锛?璇锋眰 2 涓?RX queue 鏃讹紝PMD 鏄?PASS 杩樻槸 BLOCKED锛?queue-to-core mapping 搴旇濡備綍鎻忚堪锛?```

## 瀹為獙璺緞

```text
pcap PMD
  -> rte_eth_dev_info_get()
  -> max_rx_queues / max_tx_queues
  -> flow_type_rss_offloads / reta_size
  -> rte_eth_dev_configure(rxq=2)
  -> PASS_QUEUE_CONFIG or BLOCKED_QUEUE_CONFIG
```

## 鐩綍

```text
lab-dpdk-rss-multiqueue/
鈹溾攢鈹€ app/
鈹溾攢鈹€ docs/
鈹溾攢鈹€ records/
鈹溾攢鈹€ reports/
鈹溾攢鈹€ scripts/
鈹斺攢鈹€ tools/
```

## 楠屾敹

```text
PASS_QUEUE_CONFIG or BLOCKED_QUEUE_CONFIG
PASS_RSS_QUERY or BLOCKED_RSS
PASS_QUEUE_TO_CORE_DOC
```

## 鏂囨。鍏ュ彛

- `04_DEEP_LEARNING.md`
- `02_TEST_AND_VERIFY.md`
- `03_RESULT_ANALYSIS.md`

## 杈圭晫

Phase 2 鏄?capability probe锛屼笉鏄敓浜х骇 RSS 璋冧紭銆傚鏋?pcap PMD 鎴?VMware/vmxnet3 涓嶆敮鎸佺洰鏍囪兘鍔涳紝瑕佹槑纭啓鎴?BLOCKED evidence銆?