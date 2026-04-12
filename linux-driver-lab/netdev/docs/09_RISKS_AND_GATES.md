# 09. 风险与阶段 Gate

## 主要风险

### 风险1：过早引入平台复杂度
表现：一开始就深陷交叉编译、QEMU 启动、rootfs 细节，导致 netdev 主线推进缓慢。

### 风险2：把 ring/DMA 放在 skb 之前
表现：理解顺序颠倒，后期对 RX/TX 语义理解不牢。

### 风险3：把 NAPI 当成普通 API 学
表现：只会写 `napi_schedule()`，却讲不清中断风暴、budget 与 re-enable 语义。

### 风险4：低估 RX replenishment
表现：会“收包”，但说不清 buffer 生命周期和 refill 跟不上时会怎样。

### 风险5：Stage05 混入太多工作
表现：`virtio-net` 精读、平台参数化、ARM64 迁移同时发生，节奏失控。

## Gate 建议

### Gate A：Stage02 结束后停下来复盘
如果此时还讲不清 `skb` 与 TX/RX 软件闭环，就不要进入 NAPI。

### Gate B：Stage03 结束后做中期检查
如果 NAPI 观测还不稳定，不要急着推进到 ring/DMA。

### Gate C：Stage04 结束后做阶段评审
只有在 ring、refill、DMA 生命周期都能讲清楚时，再进入 `virtio-net`。
