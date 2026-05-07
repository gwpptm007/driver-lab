# 01_V17_TO_MODERN_REVIEW_PLAN

## 对照维度

| 维度 | DPDK v17 | 当前 track |
|---|---|---|
| 构建 | make | meson/ninja |
| 设备绑定 | igb_uio/uio | uio_pci_generic/vfio-pci |
| 回内核 | KNI | tap/AF_XDP/virtio/vhost 对照 |
| 数据路径 | rx_burst/tx_burst | rx_burst/tx_burst |
| 媒体面 | UDP 收发/改写/转发 | fastpath/media-gateway-lite |

## 面试讲法目标

能够清楚说明：

1. 为什么 DPDK 绕过内核协议栈；
2. PMD、hugepage、mempool、mbuf、burst 的关系；
3. 旧项目中的 UDP 媒体面如何映射到当前 modern DPDK 实现；
4. KNI 为什么不再是优先路线；
5. 当前项目如何证明能力，而不只是写 demo。
