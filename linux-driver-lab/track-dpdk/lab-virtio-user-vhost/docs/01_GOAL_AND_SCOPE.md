# 01_GOAL_AND_SCOPE

## 目标

本实验验证 DPDK 纯用户态虚拟链路：

```text
frontend: dpdk-testpmd --vdev=net_virtio_user0,path=/tmp/dpdk-vhost-user0
                     │
                     │ vhost-user socket
                     ↓
backend : dpdk-testpmd --vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,client=0
```

上一站 `lab-vhost-user-basic` 只证明 backend 能创建 socket；本站在此基础上证明 frontend 能接进来。

## 适用范围

- 复用 hugepage/testpmd 环境
- 使用 `--no-pci`，不操作物理网卡
- 不影响 ens33 SSH 管理口
- 把问题聚焦在 `virtio-user <-> vhost-user` 协商和 testpmd 运行证据上

## 下一步

进入 `lab-dpdk-l2-forwarding` 自写 L2 forwarding 应用替代 testpmd