# 04_TX_PATH

## 目标

把 `virtio_net` 的 TX 路径读成一条可讲清楚的主线，而不是记一堆散函数。

## 这轮重点回答

1. net stack 从哪里进入驱动 TX？
2. skb 在驱动里如何被组织进 virtqueue？
3. kick/notify 的触发点在哪里？
4. TX completion / reclaim 如何回来？
5. 真正的“发送完成”在软件结构上体现在哪里？

## 推荐做法

### 先找入口
先从 `net_device_ops` 里确定 TX 入口函数，再往下跟：

- `ndo_start_xmit`
- TX queue 选择/发送入口
- virtqueue 提交
- notify / kick
- completion / free / reclaim

### 再画路径
把下面这条线画成自己的版本：

```text
protocol stack
  -> ndo_start_xmit
    -> driver TX prepare
    -> virtqueue add buffer
    -> notify/kick
    -> host/backend 消费
    -> completion/reclaim
```

## 对照你自己的 stage

建议把这一篇和这些阶段对照看：

- `stage02_skb_path`
- `stage04_ring_dma`
- `stage09_multi_queue_scaling`
- `stage10_msix_per_queue_irq`
- `stage13_offload_basics`

## 本篇交付建议

- 一张 TX 路径图
- 一份“入口函数 -> 关键 helper -> reclaim 回来点”的清单
- 一段你自己的文字解释：`virtio_net` 的 TX 和你教学驱动最大的不同是什么
