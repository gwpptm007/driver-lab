# DPDK_INTERVIEW_NOTES

## 1. 30 秒版本

```text
我最近把 DPDK 做成了一条完整实验和项目线：先在 Ubuntu/VMware 上完成 hugepage、devbind、vmxnet3 PMD 和 testpmd 验证，再做 vhost-user/virtio-user 本机虚拟链路，然后从 testpmd 过渡到自写 l2fwd-lite、fastpath-lite 和 media-gateway-lite。代码里实现了 ethdev 初始化、mbuf/mempool、rx/tx burst、Ethernet/ARP/IPv4/UDP 分类、UDP-only 策略、rewrite 框架和 per-port/per-rule/drop reason stats。最后我还把以前 DPDK v17 媒体面项目经验和现代 DPDK API 做了迁移复盘。
```

## 2. 2 分钟版本

```text
这条 DPDK track 我是按真实工程能力拆开的。第一步不是直接写业务代码，而是先把环境打通，包括 hugepage、dpdk-devbind、vmxnet3 PMD、uio_pci_generic 和 testpmd stats。第二步做 vhost-user backend 和 virtio-user frontend，理解用户态虚拟网卡和 socket 后端的关系。第三步开始写 C 程序，从 l2fwd-lite 做端口初始化、mempool、RX/TX queue、rx_burst/tx_burst 和 stats。第四步做 fastpath-lite，加入 Ethernet/ARP/IPv4/UDP 分类、UDP-only 过滤、MAC/IP/UDP 端口改写框架和软件统计。第五步做 media-gateway-lite，把代码拆成 config、port、packet、rule、stats 模块，更接近真实媒体面网关。

因为我之前做过 DPDK v17 的媒体面项目，所以最后还专门做了一站 v17 legacy review，把旧项目里的 uio/KNI、UDP 收发、ARP/IP/UDP rewrite、网元方向转发和现代 DPDK 的 vfio/uio、vhost-user/virtio-user、ethdev API 做映射。当前 media-gateway-lite 已完成 smoke 和 UDP-only drop path，真实 traffic/forward/rewrite 我作为后续补测项继续完善。
```

## 3. 面试官问：你 DPDK 到底做到了什么？

答法：

```text
我不是只跑了 testpmd。我先用 testpmd 验证 PMD 和环境，然后自己写了 l2fwd-lite 和 fastpath-lite。核心路径包括 rte_eal_init、mempool、port configure、rx/tx queue setup、rx_burst/tx_burst、eth stats。同时我把 fastpath 按解析、规则、改写和统计拆开，支持 ARP/IPv4/UDP 分类、UDP-only 策略、MAC/IP/UDP 端口改写框架和 drop reason 统计。后面做了 media-gateway-lite，把它组织成一个简化媒体网关原型。
```

## 4. 面试官问：为什么 vfio-pci 没用，而用了 uio_pci_generic？

答法：

```text
测试机是 VMware Workstation 虚拟机，guest 里 IOMMU 条件不一定满足，所以 vfio-pci 绑定会失败。这种情况下我没有强行绕，而是用 uio_pci_generic 完成 DPDK PMD 接管。真实服务器上如果 IOMMU 和 vfio 条件满足，优先 vfio-pci；实验环境里 uio_pci_generic 更稳定。
```

## 5. 面试官问：vhost-user / virtio-user 是什么关系？

答法：

```text
vhost-user 更像后端，通常由 DPDK 程序通过 UNIX socket 提供数据面后端；virtio-user 是用户态 virtio 前端，可以不用真正启动 VM，就在本机通过 socket 接到 vhost-user backend。它适合做本机虚拟链路验证，也能帮助理解 QEMU virtio-net 和 vhost-user 的数据路径。
```

## 6. 面试官问：你 media-gateway-lite 做完了吗？

答法要诚实：

```text
media-gateway-lite 当前完成了项目骨架、模块拆分、双 vdev smoke 和 UDP-only drop path 验证；真实 UDP traffic、forwarding、rewrite 的 records 还在后续补测。这个阶段我不会把它夸成完整生产网关，但它已经能体现 DPDK 数据面工程组织方式，后续补真实流量闭环即可升级到 PASS_FORWARDING/PASS_REWRITE。
```

## 7. 面试官问：DPDK v17 和现代 DPDK 有什么区别？

答法：

```text
v17 时代很多项目会用 igb_uio、KNI，代码和构建方式也更旧。现代 DPDK 更强调 meson/ninja、vfio-pci、安全隔离、vhost-user/virtio-user、eventdev/flow API 等。KNI 过去常用于把流量或控制路径回注到内核，但现在很多场景会用 virtio-user、tap、AF_XDP 或更清晰的控制面/数据面分离方案替代。迁移时重点不是机械替换 API，而是重新梳理 PMD、内存、队列、控制面、统计和部署方式。
```

## 8. 简历项目一句话

```text
基于 DPDK 21.11 构建用户态数据面与媒体网关原型，完成 vmxnet3 PMD 接管、vhost-user/virtio-user、自研 l2fwd-lite/fastpath-lite、media-gateway-lite smoke 验证，并结合 DPDK v17 媒体面经验整理 KNI/UIO/VFIO/vhost 迁移复盘。
```
