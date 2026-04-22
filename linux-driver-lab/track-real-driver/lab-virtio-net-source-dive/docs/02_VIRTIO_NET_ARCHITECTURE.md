# 02_VIRTIO_NET_ARCHITECTURE

阅读 `virtio_net` 前，先把它放到以下框架中：

- 设备模型层：`virtio_driver` / virtio bus / feature negotiation
- 网络设备层：`net_device` / `net_device_ops` / ethtool ops
- 数据路径层：TX queue / RX queue / NAPI / skb / XDP
- 虚拟化协同层：guest front-end ↔ host/back-end

本 Lab 先聚焦前三层，不展开完整 host/back-end。
