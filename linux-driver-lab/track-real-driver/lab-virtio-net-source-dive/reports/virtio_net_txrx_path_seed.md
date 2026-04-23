# virtio_net_txrx_path_seed

> 这是一份“画路径图时应该至少包含哪些节点”的种子清单。

## TX path 至少应包含

1. 网络栈发包入口
2. `ndo_start_xmit`
3. queue 定位/选择
4. 向 virtqueue 提交
5. notify / kick
6. completion / reclaim

## RX path 至少应包含

1. RX buffer 准备/refill
2. callback / 事件触发
3. napi schedule
4. poll(budget)
5. 从 queue 取数
6. skb 构建
7. GRO/checksum/XDP 边界
8. 上送协议栈
9. recycle / refill

## 最后一定要补

- 每个节点和自己哪个 stage 最像
- 哪些地方是真实驱动比教学驱动多出来的复杂度
