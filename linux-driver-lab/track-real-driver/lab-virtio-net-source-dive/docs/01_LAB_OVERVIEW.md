# 01_LAB_OVERVIEW

本 Lab 不是继续写一个新的教学驱动，而是把已经完成的 `netdev/stage00~stage14` 作为知识基线，切换到真实 Linux 驱动源码分析。

## 为什么 stage14 之后不再继续 stage15

`stage` 适合承载课程式线性推进，但不适合长期承载：

- 真实驱动源码专题
- 问题导向的 patch 实验
- 多主题并行的 Track 研究

因此：

- `netdev/stage00~stage14` 保持为第二阶段主线
- `stage14` 之后切到 `track / lab / project`
