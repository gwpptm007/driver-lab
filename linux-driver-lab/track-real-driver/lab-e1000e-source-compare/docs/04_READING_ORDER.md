# 04_READING_ORDER

## 第一轮：先建立骨架

建议先看：
1. 驱动入口和注册点
2. probe/remove
3. 私有结构体
4. netdev 注册
5. ethtool / stats 入口

## 第二轮：再看 TX / RX

建议回答：
- TX 从哪进
- descriptor / queue 生命周期怎么组织
- completion / reclaim 怎么回
- RX buffer / skb build / recycle 怎么组织

## 第三轮：再收 IRQ / NAPI / stats

建议回答：
- 中断与 poll 的关系
- NAPI 在这里的角色
- stats / ethtool 如何帮助观测
- 和 `virtio_net` 的主要差异是什么
