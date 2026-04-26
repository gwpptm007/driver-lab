# 04_BASELINE_AND_BEFORE_AFTER_PLAN

## 为什么这个实验必须重视 before/after

小 patch 实验最怕的不是“改不动”，而是：

- 改了但说不清变化
- 改了但没有证据
- 改了但 workload 不一致，无法对照

所以这一轮必须把 before/after 做成标准动作。

## baseline 来源

优先级建议：

### 第一优先级
直接复用 `lab-virtio-net-runtime-observe` 已经跑过的：
- `ethtool -S`
- workload
- dmesg
- trace/log

### 第二优先级
如果前一轮数据不完整，再在当前环境重跑一轮轻量 baseline。

## 建议的对照内容

### before
- `ethtool -S <ifname>`
- `ethtool -i <ifname>`
- workload 输出
- 关键 dmesg/trace
- patch 前源码位置说明

### after
- 同样一组 `ethtool -S`
- 同样 workload
- 同样日志采集
- patch 后 review note

## 当前最小通过标准

- 能做出一份 before/after stats diff
- 能说明差异是否符合预期
- 能把差异和 patch 点对应起来
