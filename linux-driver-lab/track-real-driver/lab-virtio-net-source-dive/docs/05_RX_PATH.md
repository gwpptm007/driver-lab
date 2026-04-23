# 05_RX_PATH

## 目标

把 RX 路径读成“buffer 预投递 -> poll -> 构建 skb -> 上送协议栈”的完整闭环。

## 这轮重点回答

1. receive buffer 在什么时候被预投递？
2. 谁负责补 buffer / refill？
3. 收到包后如何切到 poll/NAPI 上下文？
4. poll 中如何消费 virtqueue 上的数据？
5. skb 是如何构建出来的？
6. GRO / checksum / XDP 在哪里进入？

## 推荐阅读方法

### Step 1：先找 refill 相关路径
先看 receive queue 初始化后，buffer 是谁投递进 virtqueue 的。

### Step 2：再看 poll
抓住 poll 主入口，回答：
- budget 怎么用
- 一次 poll 里做了哪些事
- 哪些条件会重新使能中断 / 结束 poll

### Step 3：最后看 skb 构建与上送
重点判断：
- 数据来自 page / linear buffer 还是其他形态
- checksum/GRO/XDP 的判断点在哪里
- RX completion/recycle 在哪里体现

## 对照你自己的 stage

这一篇优先和这些阶段对照：
- `stage03_napi_poll`
- `stage04_ring_dma`
- `stage11_page_pool_rx`
- `stage13_offload_basics`
- `stage14_xdp_basics`

## 本篇交付建议

- 一张 RX 路径图
- 一张“buffer/refill/poll/recycle”关系图
- 一段你自己的总结：真实 `virtio_net` 的 RX 比你的教学驱动多了哪些复杂度
