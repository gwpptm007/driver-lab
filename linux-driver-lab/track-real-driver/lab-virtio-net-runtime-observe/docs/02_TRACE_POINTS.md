# 02_TRACE_POINTS

## 原则

先做 **小而稳** 的观测，不求第一版就把所有点都挂满。

## 第一批推荐观测点

### A. 与 TX 相关
- 发包入口相关函数
- 可能的 queue submit 关键点
- 可能的 completion / reclaim 关键点

### B. 与 RX 相关
- queue/callback/wakeup 相关入口
- `virtnet_poll` 或等价 poll 入口
- RX 处理/取数关键点
- refill/recycle 相关关键点

### C. 与公共推进相关
- NAPI 相关点
- callback / schedule 相关点
- 可配合 `ethtool -S` 的统计采样

## 建议做法

第一版不要假设所有函数都一定可 trace。  
更稳的做法是：

1. 用脚本先找候选函数
2. 在测试机上确认实际存在的符号
3. 再生成当前轮次的 trace 清单

## 推荐使用方式

### 方案 1：function tracing / function_graph
优点：
- 快速看到函数是否真的走到

缺点：
- 噪音可能偏大

### 方案 2：定点函数 filter
优点：
- 更适合和 `source-dive` 中的路径图对照

缺点：
- 需要先挑关键函数

## 第一版通过标准

不是“追全所有函数”，而是至少能稳定得到：

- 一组 TX 相关痕迹
- 一组 RX 相关痕迹
- 一组 NAPI/事件推进相关痕迹
