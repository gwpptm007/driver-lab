# 08_SHARE_NOTES

## 这个专题怎么讲

### 1. 为什么在 `virtio_net` 之后看 `e1000/e1000e`
因为需要补“第二个真实驱动视角”，避免真实驱动理解只停留在 virtio。

### 2. 这个专题的价值
- 建立传统 PCI NIC 驱动视角
- 和 `virtio_net` 做对照
- 和自己 `netdev/stage00~stage13` 再做一轮映射

### 3. 做完后会发生什么
做完这个专题之后，再切：
- `track-virtual-net/`
会更稳，因为你已经同时拥有：
- 半虚拟化 NIC 视角
- 传统 NIC 驱动视角
