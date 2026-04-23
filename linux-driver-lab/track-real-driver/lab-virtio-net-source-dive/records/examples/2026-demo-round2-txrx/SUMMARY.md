# SUMMARY

## 本轮目标
把 TX/RX 主路径建立成“可以复述”的图，而不是只记零散函数名。

## TX 侧当前理解
- 入口是 netdev 的发包回调
- 中间围绕 queue/virtqueue 组织
- notify/kick 不是孤立动作，而是队列推进的一部分
- completion/reclaim 要和发包入口一起看，不能只看 enqueue

## RX 侧当前理解
- RX 不是“收到包就直接上送”，中间隔着 callback / poll / budget
- skb 构建、GRO/checksum、XDP 入口都要挂在 RX 路径图里
- 如果只盯单个函数，很容易看散

## 当前和自己 stage 的主要差异感受
- 自己的教学驱动更强调概念拆解
- `virtio_net` 更强调真实资源组织、队列协同与能力边界
