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


## 当前推进点

当前应从第一站开始：

```text
lab-vmxnet3-testpmd
```

这一站已经结合 `docs/00_ENVIRONMENT_PREPARE.md` 中的测试机环境收敛为：

```text
ens33  = e1000/NAT/SSH 管理口，不动
ens192 = vmxnet3/0000:0b:00.0，DPDK 测试口
```

进入方式：

```bash
cd lab-vmxnet3-testpmd
cat START_HERE.md
```
