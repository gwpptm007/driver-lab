# 01_GOAL_AND_SCOPE

## 目标

本实验目标是完成 DPDK vhost-user backend 的最小验证：

```text
dpdk-testpmd
  + net_vhost vdev
  + UNIX domain socket
  + port info/stats evidence
```

## 不做什么

本实验刻意不做下面这些事情：

- 不连接 QEMU guest。
- 不连接 `net_virtio_user` frontend。
- 不要求 RX/TX 非 0。
- 不操作 `ens192/vmxnet3`。
- 不执行 `dpdk-devbind.py`。

这些内容会在后续实验中推进。

## 通过后的意义

通过后说明当前测试机具备继续做 DPDK 虚拟化数据面的基础能力：

```text
hugepage/testpmd 已可用
vhost-user backend 可启动
socket 通道可创建
records 证据链可复用
```
