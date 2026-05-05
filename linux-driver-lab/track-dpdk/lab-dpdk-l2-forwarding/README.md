# lab-dpdk-l2-forwarding

> 所属：`track-dpdk/`  
> 角色：从 `testpmd` 过渡到自己的 DPDK C 数据面程序。

## 一句话定位

实现并验证第一个可编译、可运行、可留证的 DPDK L2 forwarding C app：`l2fwd-lite`。

## 本 lab 为什么放在这里

前面三站已经完成：

```text
lab-vmxnet3-testpmd      -> 真实 VMXNET3 PMD + testpmd smoke
lab-vhost-user-basic     -> vhost-user backend socket smoke
lab-virtio-user-vhost    -> virtio-user 与 vhost-user 本机闭环
```

现在开始从工具型验证切换到自研代码：

```text
EAL
mempool
mbuf
ethdev
rx_queue / tx_queue
rx_burst / tx_burst
software stats
```

## 当前测试机适配

当前 VMware 测试机只有一个 DPDK 专用 VMXNET3 口：

```text
管理网卡：ens33 / e1000 / 0000:02:01.0
DPDK网卡：ens192 / vmxnet3 / 0000:0b:00.0
推荐绑定：uio_pci_generic
```

因此本 lab 分两个验收层级：

```text
PASS_SMOKE       单端口初始化 + 数据面循环 + stats 输出
PASS_FORWARDING  双端口或外部发包源下 RX/TX 互转非 0
```

在当前测试机上，先做到 `PASS_SMOKE` 即可进入下一阶段。

## 推荐阅读顺序

1. `START_HERE.md`
2. `docs/01_GOAL_AND_SCOPE.md`
3. `docs/02_EXECUTION_FLOW.md`
4. `docs/03_ACCEPTANCE.md`
5. `docs/04_C_APP_STRUCTURE.md`
6. `docs/05_TEST_MACHINE_RUNBOOK.md`
7. `reports/lab-dpdk-l2-forwarding_exec_board.md`
