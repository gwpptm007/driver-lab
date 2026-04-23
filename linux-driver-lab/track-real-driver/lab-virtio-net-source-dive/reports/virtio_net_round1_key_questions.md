# virtio_net_round1_key_questions

## Round1 必须能回答的问题

1. `virtio_net` 在设备模型层、netdev 层、数据路径组织层分别是什么角色？
2. 为什么 `virtnet_info` 是第一轮最重要的私有结构锚点？
3. 为什么 probe 不能只看成“register_netdev 之前的初始化”？
4. queue / napi 的关系为什么要先于 TX/RX 细节理解？
5. 自己的 `stage01/stage03/stage09/stage10/stage11` 在这里分别映射到什么？

## 用途

- 自测
- 评审前自查
- 组内分享前复盘
