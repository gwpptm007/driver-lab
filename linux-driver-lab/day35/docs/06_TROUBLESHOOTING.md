# Day35 常见问题

## 1. 脚本提示找不到某天的 records

说明对应 day 的 `records/` 缺失或目录名不符合默认规则。

Day35 依赖：

- day29/records/*
- day30/records/*
- day31/records/*
- day32/records/*
- day33/records/*
- day34/records/*

## 2. 生成的 report 中某项是 N/A

说明当前仓库里的 records 中没有该字段，或者该天本来就是失败现场。

这不一定是 Day35 脚本错误，也可能是原始实验没有产出对应指标。

## 3. 为什么 Day33 仍然是 fail

因为当前基线里的 Day33 记录本身就是“主路径通过、trace 采集失败”的失败现场。

Day35 会如实保留这个结论，而不是把它改写成通过。
