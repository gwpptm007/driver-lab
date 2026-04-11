# Day32 设计与风险

## 1. 设计取舍

Day32 刻意把优化点放在**bench 工具**而不是驱动里，原因是：

- 驱动侧主链路在 day31 已经过一轮验证
- 工具侧热循环更容易做小改动和前后对比
- 不会把问题混到 QEMU EDU 仿真开销里

## 2. 风险

- baseline/optimized 差异过小，说明 workload 不够稳定或样本不够大
- host perf 主要采到的是 QEMU 自身，而不是 guest tool 语义
- guest 内没有 perf，当前 day32 的热点证据主要依赖宿主 perf 包裹整个 QEMU 进程

## 3. 风险缓解

- 先用 timing 结果确认优化方向
- 再用 perf 辅助解释热点
- 保持 workload 固定，避免前后不可比
