# 01_PROJECT_GOAL

## 项目目标

把三个 Lab 收成一个可展示项目：

```text
guest virtio_net
  -> QEMU/vhost backend
  -> tap
  -> Linux bridge
  -> host / another guest
```

## 项目价值

这条项目线把你从：

- 单个 guest 驱动函数
- 单个 tap/bridge 实验

推进到：

- host/guest 协同路径
- userspace backend vs kernel vhost backend
- guest-to-guest flow
- 后续 DPDK/vhost-user 的基础
