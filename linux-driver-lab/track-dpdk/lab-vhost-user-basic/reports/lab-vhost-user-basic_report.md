# lab-vhost-user-basic_report

## 实验定位

本实验承接 `lab-vmxnet3-testpmd`，从真实 PMD smoke test 进入 DPDK 虚拟化数据面。

本阶段只验证 vhost-user backend，不接 virtio frontend。

## 关键设计

```text
dpdk-testpmd
  ├── --no-pci
  └── --vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0
```

## 验收证据

- `TESTPMD_COMMAND.txt`
- `VHOST_SOCKET.txt`
- `TESTPMD_VHOST.log`
- `REVIEW_BUNDLE.md`

## 下一步

进入 `lab-virtio-user-vhost`，让 `virtio-user` frontend 连接当前 vhost-user backend。
