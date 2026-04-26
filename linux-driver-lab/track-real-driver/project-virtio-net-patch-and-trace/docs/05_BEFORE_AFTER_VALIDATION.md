# 05_BEFORE_AFTER_VALIDATION

## 为什么这是项目核心

这个项目的价值，不在于 patch 文件存在，  
而在于你能证明：

- 改动前是什么
- 改动后是什么
- 差异是否符合预期
- 差异是否值得保留

## 最少要有的 before/after

### before
- `ethtool -i`
- `ethtool -S`
- `ip -s link`
- ping / iperf 基本输出
- 必要的 dmesg / trace

### after
- 同样一组数据
- 同样 workload
- 同样记录方式

## 最小通过标准

- 至少有一份 diff
- 至少能说清楚一项可解释差异
- patch、diff、review note 三者能对上
