# 03_DPDK_CORE_CONCEPTS

## 核心概念

- EAL
- hugepage
- vfio-pci / uio
- PMD
- mempool
- mbuf
- port
- rx/tx queue
- burst RX/TX
- testpmd
- vhost-user
- virtio-user

## 学习顺序

不要先写复杂 C。先跑通：

```text
hugepage -> bind -> testpmd -> vhost-user -> virtio-user -> L2 C app
```
