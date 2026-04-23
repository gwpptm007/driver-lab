# 09_NEXT_STEP_PATCH_POINTS

## 这个 Lab 完成后，不是立刻换题，而是继续往“可验证的小实验”推进

下面这些点最适合做后续小 patch / trace / 观测实验。

## 方向 1：统计与观测增强
### 适合做什么
- 给关键路径加最小统计
- 验证某个 TX/RX/reclaim 点是否真的经过
- 对 queue / napi / notify / completion 做事件级记录

### 为什么适合
- 侵入性小
- 验证价值高
- 容易和你现有 `records/` 体系结合

## 方向 2：queue / napi 关系验证
### 适合做什么
- 验证单队列与多队列的对象关系
- 验证中断 -> poll -> re-enable 的实际顺序
- 用 trace/ftrace 观察关键函数是否符合预期

## 方向 3：ethtool / feature 面观察
### 适合做什么
- 对 stats / channels / feature 入口做最小阅读与观测
- 建立“用户态工具 -> 驱动接口 -> 内核状态”的路径图

## 方向 4：XDP 真入口验证
### 适合做什么
- 对 attach/detach 生命周期做观测
- 看 XDP 与普通 skb path 在 RX 主线里的分界

## 后续推荐顺序

1. `lab-real-driver-ethtool-stats`
2. `lab-real-driver-small-patch`
3. 再考虑切到 `track-virtual-net/lab-virtio-vhost-kick-notify`
