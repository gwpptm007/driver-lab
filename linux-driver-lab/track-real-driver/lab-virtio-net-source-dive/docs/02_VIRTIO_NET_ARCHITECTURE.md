# 02_VIRTIO_NET_ARCHITECTURE

## 先把 virtio_net 放到四层框架里看

### 1. 设备模型层
- `virtio_driver`
- virtio bus
- feature negotiation

### 2. 网络设备层
- `net_device`
- `net_device_ops`
- ethtool ops

### 3. 数据路径层
- send/receive queue
- virtqueue
- NAPI
- skb
- offload / XDP 入口

### 4. 虚拟化协同层
- guest front-end
- host/back-end

本 Lab 先聚焦前 3 层，不展开完整 host/back-end。

## 第一轮先盯的结构体

建议先 grep / 阅读这些类型与它们的字段用途：

- `struct virtnet_info`
- `struct receive_queue`
- `struct send_queue`
- `struct virtqueue`
- `struct napi_struct`
- `struct net_device`

## 你要回答的最小问题

1. 驱动私有数据挂在哪里？
2. TX/RX queue 怎么组织？
3. NAPI context 和 queue 是怎么关联的？
4. `net_device_ops` / ethtool ops 从哪里接出去？
5. feature / control plane 是集中在什么位置处理的？
