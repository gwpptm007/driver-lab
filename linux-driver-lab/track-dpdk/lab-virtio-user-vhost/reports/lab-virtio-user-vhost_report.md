# lab-virtio-user-vhost_report

## 实验定位

本实验承接 `lab-vhost-user-basic`，从单独 vhost-user backend smoke test 进入本机 `virtio-user <-> vhost-user` 互联验证。

## 关键设计

```text
backend testpmd
  ├── --no-pci
  └── --vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0

frontend testpmd
  ├── --no-pci
  └── --vdev=net_virtio_user0,path=/tmp/dpdk-vhost-user0,queues=1,server=0
```

## 验收证据

- `TESTPMD_COMMANDS.txt`
- `TESTPMD_BACKEND.log`
- `TESTPMD_FRONTEND.log`
- `VHOST_SOCKET.txt`
- `REVIEW_BUNDLE.md`

## 和下一站的关系

这一站仍然使用 testpmd。下一站 `lab-dpdk-l2-forwarding` 开始写 C app：

```text
rte_eal_init
rte_pktmbuf_pool_create
rte_eth_dev_configure
rte_eth_rx_queue_setup
rte_eth_tx_queue_setup
rte_eth_rx_burst / rte_eth_tx_burst
```
