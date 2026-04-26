# 03_COMPARE_DIMENSIONS

## 推荐从这 6 个维度做对照

### 1. 设备模型
- `virtio_net`：virtio bus / 半虚拟化设备
- `e1000/e1000e`：PCI 传统 NIC 驱动视角

### 2. 驱动骨架
- probe/remove 结构
- 私有结构体组织
- netdev 注册方式

### 3. TX / RX 主路径
- 入口函数
- queue / descriptor / 提交 / 回收
- RX buffer / skb 构建 / recycle

### 4. IRQ / NAPI / 事件推进
- 事件来源
- callback / 中断语义
- NAPI 的角色
- queue 与 poll 的关系

### 5. ethtool / stats / control plane
- 暴露哪些能力
- 哪些统计项最值得观察
- 哪些 control-plane 接口最有对照价值

### 6. 与自己 stage 的映射
- `stage03_napi_poll`
- `stage09_multi_queue_scaling`
- `stage10_msix_per_queue_irq`
- `stage11_page_pool_rx`
- `stage12_ethtool_control_plane`
- `stage13_offload_basics`
