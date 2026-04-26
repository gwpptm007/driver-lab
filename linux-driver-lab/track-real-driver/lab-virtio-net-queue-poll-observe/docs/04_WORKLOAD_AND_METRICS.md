# 04_WORKLOAD_AND_METRICS

## workload 建议顺序

### 1. idle
用于建立静态/低活动基线。

### 2. ping
用于建立最小有流量闭环。

### 3. iperf3
用于把 poll / RX 节奏拉起来，让差异更明显。

## 建议收集的指标

### 最小必收
- `ethtool -i`
- `ethtool -S`
- `ip -s link`
- 关键 trace/log
- 运行窗口内的 before/after 计数

### 当前重点指标
- RX 相关 delta
- poll 相关证据
- 有无明显的 idle / ping / iperf 差异

## 输出建议

每一轮 records 里至少有：
- `window_note.md`
- `stats_before.txt`
- `stats_after.txt`
- `stats.diff`
- `trace.txt`
- `SUMMARY.md`
