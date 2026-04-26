# track-dpdk

> DPDK 用户态数据面主线

## 一句话定位

从 vmxnet3/testpmd 起步，逐步进入 vhost-user、virtio-user、自写 L2 forwarding C app，最终收成 user-space fastpath 项目。

## 阶段列表

1. `lab-vmxnet3-testpmd` — VMware vmxnet3 + DPDK testpmd 基础
2. `lab-vhost-user-basic` — QEMU virtio-net + DPDK vhost-user backend
3. `lab-virtio-user-vhost` — DPDK virtio-user + vhost-user 用户态互联
4. `lab-dpdk-l2-forwarding` — 第一个 DPDK C 数据面程序
5. `project-user-space-fastpath` — DPDK 用户态 fastpath 收尾项目

## 推荐方式

按 `ROADMAP.md` 顺序推进，每个 Lab 都要形成：

- README
- START_HERE
- docs
- scripts
- records
- reports
- acceptance
