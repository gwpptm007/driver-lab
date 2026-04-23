# SUMMARY

## 本轮目标
先建立 `virtio_net` 的整体骨架认知，不急着把所有 helper 都追完。

## 当前观察到的层次

### 1. 设备模型层
可以先把 `virtio_net` 看作 “virtio bus 上的网络驱动前端”。

### 2. netdev 接入层
本轮重点不在每个细节，而在确认：
- netdev 是何时分配/初始化/注册的
- 私有结构体如何与 netdev 关联
- `net_device_ops` / ethtool ops 如何挂接

### 3. 数据路径层
从整体上确认：
- TX 与 RX 都围绕 queue/virtqueue 组织
- NAPI 是 RX 处理的重要承接点
- 后续 Round2 再深入 TX/RX 细节

## 本轮最重要的结论
`lab-virtio-net-source-dive` 的第一轮不求“读完”，而求“建立骨架 + 找到入口 + 建立自己的函数分组方式”。

## 下一轮要继续追的方向
- TX path
- RX path
- callback / poll / notify 的衔接
