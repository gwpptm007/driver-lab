# 14_FUNCTION_GROUPING_GUIDE

> 这份文档教你如何把 `virtio_net.c` 的函数列表从“一个大文件”拆成“可记忆的分组”。

## 建议分为 6 组

### 1. 设备与注册组
关注：
- driver registration
- probe/remove
- netdev 分配与注册
- feature 协商总入口

### 2. 队列与初始化组
关注：
- queue pair 初始化
- virtqueue 建立
- napi 挂接
- queue 资源释放

### 3. TX 组
关注：
- `ndo_start_xmit`
- enqueue / kick / reclaim
- TX completion

### 4. RX 组
关注：
- refill / receive buffer
- poll
- skb 构建
- GRO/checksum/XDP 边界

### 5. 中断与通知组
关注：
- callback
- napi schedule
- IRQ / notify / kick 配合

### 6. 控制面与能力组
关注：
- ethtool
- stats
- channels / queue 参数
- feature / offload / XDP

## 使用建议

先通过 `build_grouped_function_index.sh` 生成分组草稿，再人工补“为什么重要”和“和 stage 的对应关系”。
