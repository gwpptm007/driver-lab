# 07_SHARE_AND_INTERVIEW_NOTES

## 这个实验怎么讲最自然

### 1. 为什么不是直接改主路径
因为在真实驱动里，第一次实验更应该：
- 低风险
- 好验证
- 有闭环

所以先选 `ethtool / stats`，而不是一上来改重 TX/RX。

### 2. 这个实验承接了什么
- `lab-virtio-net-source-dive` 提供了源码理解
- `lab-virtio-net-runtime-observe` 提供了运行期 baseline
- `lab-virtio-net-ethtool-stats-mini-patch` 把前两者推进成真实 patch

### 3. 这个实验的价值
- 它让“读源码”第一次变成“改真实驱动”
- 风险比主路径 patch 低
- 但已经足够体现你对驱动控制面和 stats 面的理解
