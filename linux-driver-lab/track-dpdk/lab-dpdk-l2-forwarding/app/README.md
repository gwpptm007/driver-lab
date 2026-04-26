# app

这里放第一个 DPDK C 数据面程序。

第一版目标：

- `rte_eal_init`
- `rte_pktmbuf_pool_create`
- `rte_eth_dev_configure`
- `rte_eth_rx_queue_setup`
- `rte_eth_tx_queue_setup`
- `rte_eth_dev_start`
- `rte_eth_rx_burst`
- `rte_eth_tx_burst`
- Ctrl+C 退出并打印 stats
