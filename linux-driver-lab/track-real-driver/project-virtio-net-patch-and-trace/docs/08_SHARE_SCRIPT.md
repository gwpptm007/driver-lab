# 08_SHARE_SCRIPT

## 这个项目怎么讲

### 开场
我先做了 `virtio_net` 的源码专题和运行期观测，  
然后没有直接跳到重 patch，而是把 patch 和 trace 收成一个小项目。

### 第一部分：为什么选这个 patch 点
- 风险低
- 可验证
- 和前面 Lab 连续性强

### 第二部分：我是怎么做 before/after 的
- baseline
- workload
- stats / log / trace
- diff

### 第三部分：我如何解释 patch 的运行期意义
- 哪些是 stats 证据
- 哪些是 trace 证据
- 哪些结论是直接可见的
- 哪些只是辅助解释

### 收尾
这个项目让我不是只会“读驱动”，而是已经能做：
- 真实 patch
- 真实验证
- 真实评审说明
