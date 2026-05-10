# DPDK_RESUME_MATERIAL_FINAL

## 1. 简历项目标题

```text
DPDK 用户态数据面与媒体网关原型
```

## 2. 推荐简历 bullet：稳健版

```text
- 基于 DPDK 21.11 搭建用户态数据面实验环境，完成 hugepage、dpdk-devbind、vmxnet3 PMD、uio_pci_generic 绑定和 testpmd stats 验证，沉淀可复现脚本与 records。
- 实现 vhost-user backend 与 virtio-user frontend 本机虚拟链路实验，理解 UNIX socket 后端、virtio 用户态前端和 DPDK vdev 数据路径。
- 编写 l2fwd-lite / fastpath-lite C 程序，完成 EAL、mempool、ethdev、RX/TX queue、rx_burst/tx_burst、rte_eth_stats_get 等 DPDK 数据面基础链路。
- 设计 media-gateway-lite 原型，按 config/port/packet/rule/stats 模块拆分，支持 Ethernet/ARP/IPv4/UDP 分类、UDP-only 策略、MAC/IP/UDP 端口改写框架及 per-port/per-rule/drop reason 统计。
- 结合历史 DPDK v17 媒体面项目经验，整理 KNI、UIO/VFIO、vhost/virtio、UDP fastpath 与 rewrite 逻辑的现代化迁移复盘，用于后续真实流量转发与网关能力补强。
```

## 3. 如果简历空间有限：压缩版

```text
- 构建 DPDK 用户态数据面实验与媒体网关原型，完成 vmxnet3 PMD 接管、vhost-user/virtio-user、自研 l2fwd-lite/fastpath-lite 和 media-gateway-lite smoke 验证，支持 Ethernet/ARP/IPv4/UDP 分类、UDP-only 策略、MAC/IP/UDP 改写框架和 per-port/per-rule/drop reason 统计，并结合 DPDK v17 媒体面经验完成现代化迁移复盘。
```

## 4. 更贴近过去项目经验的版本

```text
- 参与/复盘 DPDK v17 媒体面数据路径，围绕 UDP 高速收发、ARP/IP/UDP 头部改写、KNI 回内核、uio 驱动绑定和网元方向转发展开；近期基于 DPDK 21.11 重新构建 modern DPDK 实验链路，完成 vmxnet3 PMD、vhost-user/virtio-user、fastpath-lite 与 media-gateway-lite 原型，实现旧项目经验到现代 DPDK API 和部署方式的迁移验证。
```

## 5. 面试时不能夸大的边界

不要写成：

```text
已实现完整生产级 DPDK 媒体网关
已完成高性能压测
已完成真实流量 rewrite 闭环
```

当前准确边界是：

```text
media-gateway-lite 当前为 PASS_SMOKE，真实 traffic/forward/rewrite 仍在后续补测。
```

## 6. 可放作品集的描述

```text
项目包含多个可复现实验阶段：vmxnet3/testpmd、vhost-user、virtio-user、l2fwd-lite、fastpath-lite、traffic-test、media-gateway-lite、v17 legacy review。每个阶段均包含 README、START_HERE、scripts、records、reports，便于在测试机上复现和评审。
```
