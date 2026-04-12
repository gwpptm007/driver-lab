# 05. 架构与数据路径说明

## 一、北向与南向

### 北向（尽量稳定）
- `net_device`
- `ndo_*`
- `skb`
- NAPI
- stats / debugfs / trace
- 测试脚本与 records 归档

### 南向（逐步演进）
- 软件注入
- 内部环回
- ring / descriptor
- DMA map/unmap
- `virtio-net` / ARM64 平台

## 二、Stage01~Stage04 的基本思想

**northbound 先稳定，southbound 逐步加复杂度。**

这意味着：

- 上层观测方式尽早固定
- 后端 transport 可以迭代替换

## 三、Stage05~Stage06 的替换边界

### 保留
- 上层 netdev 视角
- 测试方法
- 统计项
- 记录格式
- perf/ftrace 观测框架

### 替换
- ring / descriptor 组织
- buffer transport
- 平台与工具链
- feature 能力模型
