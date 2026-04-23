# virtio_net_round2_key_questions

## TX 方向必须能回答的问题

1. 为什么 `ndo_start_xmit` 只是入口？
2. TX 为什么必须从 queue 视角理解？
3. notify / kick 在路径中承担什么角色？
4. completion / reclaim 为什么是主路径的一部分？
5. `stage04/stage09/stage10` 在 TX 中分别映射到什么？

## RX 方向必须能回答的问题

1. 为什么不能只看 `virtnet_poll`？
2. RX 为什么必须从 queue + buffer lifecycle 理解？
3. refill / recycle 为什么不是配角？
4. GRO / checksum / XDP 在这一轮为何只先定边界？
5. `stage03/stage11/stage13/stage14` 在 RX 中分别映射到什么？

## 用途

- 自测
- 评审前自查
- 分享前复盘
