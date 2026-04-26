# 04_C_APP_STRUCTURE

## 第一个 DPDK C 程序的结构

```text
EAL init
  -> mbuf pool
  -> port configure
  -> rx/tx queue setup
  -> port start
  -> rx_burst / tx_burst loop
  -> stats / cleanup
```

## 第一版不要加的功能

- ACL
- rte_hash
- 多核 worker
- vhost-user backend
- header rewrite
- KNI
