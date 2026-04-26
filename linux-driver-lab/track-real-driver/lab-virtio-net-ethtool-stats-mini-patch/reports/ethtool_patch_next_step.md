# ethtool_patch_next_step

## 当前实验完成后最自然的下一步

### 方向 1：queue / poll 观测增强
如果这次 mini patch 已经站住，可以继续推进：
- `lab-virtio-net-queue-poll-observe/`

### 方向 2：更明确的 tracing 证据增强
把这次 patch 的前后效果和 runtime observe 进一步串起来。

### 当前不建议
- 立刻跳到重 TX/RX 主路径语义 patch
- 一次把 patch 做太大
