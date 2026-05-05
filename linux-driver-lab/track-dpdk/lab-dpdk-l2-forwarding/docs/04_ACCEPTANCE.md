# 04_ACCEPTANCE

## PASS_SMOKE

当前测试机默认验收等级。

必须满足：

```text
BUILD.log 中 l2fwd-lite 编译成功
L2FWD_SINGLE_PORT.log 中 EAL 初始化成功
L2FWD_SINGLE_PORT.log 中 mbuf pool 创建没有失败
L2FWD_SINGLE_PORT.log 中 port 0 started
L2FWD_SINGLE_PORT.log 中 available/initialized ports >= 1
L2FWD_SINGLE_PORT.log 中 enter forwarding loop
L2FWD_SINGLE_PORT.log 中出现 rte_eth_stats
L2FWD_SINGLE_PORT.log 中出现 bye
```

允许：

```text
RX/TX 为 0
notice: only one port is available
no_peer_drop 为 0 或非 0
```

原因：当前没有外部发包源，且只有一个 DPDK 口。

## PASS_FORWARDING

增强验收等级。

需要满足：

```text
至少两个 DPDK ethdev port 初始化成功
端口形成 0<->1 配对
外部或内部发包源产生流量
rx/tx/opackets/ipackets 出现非 0
tx_failed 不持续增长
程序能正常退出
```

## FAIL 条件

出现以下情况应判定失败：

```text
pkg-config libdpdk 不存在，无法编译
rte_eal_init failed
rte_pktmbuf_pool_create failed
no available DPDK ethdev ports found
rte_eth_dev_configure failed
rte_eth_rx_queue_setup failed
rte_eth_tx_queue_setup failed
rte_eth_dev_start failed
程序崩溃或不能正常退出
误绑定管理网卡 ens33
```
