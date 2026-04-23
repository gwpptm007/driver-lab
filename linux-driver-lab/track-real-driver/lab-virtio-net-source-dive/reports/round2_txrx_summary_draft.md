# round2_txrx_summary_draft

## 目的

这是一份面向评审的 Round2 汇总草稿，用来说明：

- `virtio_net` 的 TX/RX 主路径是否已经有了第一版稳定理解
- 这份理解是否能和自己的 `netdev/stage00~stage14` 建立映射
- 下一步是否应该进入 queue/NAPI/IRQ 与 feature/XDP 的更细收拢阶段

## 当前判断

当前 Round2 已经具备“从骨架认知进入主路径认知”的条件：

- TX 已经不再只被理解成 `ndo_start_xmit`
- RX 已经不再只被理解成 `virtnet_poll`
- queue / notify / completion / refill / recycle 这些节点已经进入统一视图

## TX 侧核心收获

### 1. `ndo_start_xmit` 是入口，不是全部
TX 需要同时看：

- queue 定位
- 提交到 virtqueue
- notify / kick
- completion / reclaim

### 2. queue 是 TX 的真正骨架
这和 `stage04/stage09/stage10` 的映射关系很强。

## RX 侧核心收获

### 1. `virtnet_poll` 是关键入口，但不是全部
RX 需要同时看：

- buffer 准备
- callback / napi schedule
- poll
- skb 构建
- GRO/checksum/XDP 边界
- refill / recycle

### 2. RX 必须带着 buffer lifecycle 视角阅读
这和 `stage11_page_pool_rx` 的对应最强。

## 下一步建议

Round2 之后最自然的下一步不是继续散读 helper，  
而是进入两个收口主题：

1. `docs/06_QUEUE_NAPI_IRQ.md`
2. `docs/07_FEATURES_ETHTOOL_XDP.md`

也就是把：
- 路径
- 事件模型
- 能力边界

真正收成可评审的一版。
