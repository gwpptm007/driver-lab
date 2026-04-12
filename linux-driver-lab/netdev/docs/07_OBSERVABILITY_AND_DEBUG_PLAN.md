# 07. 观测与调试计划

## 一、贯穿所有阶段的基本观测项

- netdev 注册/注销日志
- open/stop/start_xmit 计数
- RX/TX packet/byte 计数
- 错误计数
- debugfs 导出

## 二、Stage03 重点观测项

- irq count
- napi poll count
- budget exhausted count
- 每轮 poll 批处理包数
- 中断关闭/打开次数

## 三、Stage04 重点观测项

- ring head/tail
- avail/used 数量
- refill success/fail
- map/unmap 次数
- ring empty / full / stall 次数

## 四、建议工具

- `ip link`
- `ethtool -S`
- `cat /proc/interrupts`
- `cat /proc/softirqs`
- `perf top/record/report`
- `trace-cmd` / ftrace
- debugfs

## 五、注意事项

### 1. 不要让观测先于语义
先确定你要证明什么，再决定看哪个统计项。

### 2. 不要只看 host 工具输出
驱动内部自己的 stats/debugfs 同样重要。

### 3. 中断与 softirq 要分开看
网络驱动里，很多关键路径并不只在硬中断上下文内完成。
