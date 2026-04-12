# 03. 设备路线选择说明

## 一、先明确三个概念

### 1. 前端设备模型
guest 驱动面对的对象是什么：

- 教学型自研 netdev
- 简化 MMIO/shared-ring 网卡
- `virtio-net`

### 2. 后端数据通道
数据从哪里来、到哪里去：

- 软件注入
- 内部环回
- 用户态工具
- tap/tun
- shared memory

### 3. 外部联通方式
是否接真网络：

- 不接外网
- 只做本地收发
- 接 tap / bridge / NAT

## 二、当前推荐路线

### Stage01~Stage04
优先走：

- **前端：教学型自研 netdev**
- **后端：先软件注入 / 内部环回**

原因：

- 能把注意力放在 `net_device` / `skb` / NAPI / ring 上
- 不会一开始就被 virtio feature、tap 配置、QEMU 网络联通细节拖住
- 更适合做可解释、可截图、可归档的教学实验

### Stage05
引入：

- `virtio-net` 源码对照
- 必要的平台参数化
- 可选 tap 路线说明

### Stage06
再看：

- ARM64 QEMU
- 跨平台运行脚本
- 差异对比

## 三、为什么不把 tap 当成第一阶段答案

tap 很有用，但它回答的是“包怎么进出宿主机”，不是“你在 guest 里学到的设备模型是什么”。

所以：

- tap 可以用
- 但不能替代“前端设备模型设计”的思考

## 四、为什么先不直接上 virtio-net

因为一上来就会混入：

- virtqueue / vring
- feature negotiation
- backend 细节
- 更复杂的 buffer 管理

这会让第一阶段的学习目标变得不纯。
