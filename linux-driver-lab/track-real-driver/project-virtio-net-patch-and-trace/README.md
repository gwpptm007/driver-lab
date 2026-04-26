# project-virtio-net-patch-and-trace

> 所属：`track-real-driver/`

## 一句话定位

这是 `track-real-driver` 当前阶段的**收尾小项目**：

> **围绕 `virtio_net` 完成一次真实小 patch，并用 runtime / trace / before-after 证据把这次改动收成一个可评审、可分享、可继续扩展的项目成果。**

它不是新的基础 Lab，而是把前面的几条线整合起来：

- `lab-virtio-net-source-dive/`
- `lab-virtio-net-runtime-observe/`
- `lab-virtio-net-ethtool-stats-mini-patch/`
- `lab-virtio-net-queue-poll-observe/`

## 为什么现在做这个项目

前面几条线已经分别提供了：

- 源码理解与映射
- 运行期 baseline
- queue/poll 事件推进证据
- 第一个真实小 patch 的实验入口

继续把它们拆开做，容易越来越碎。  
把它们收成一个小项目，更像真实工程与对外作品。

## 项目目标

1. 选一个低风险、可验证的 `virtio_net` 小 patch 点
2. 形成真实 patch
3. 收集 before / after 证据
4. 用 trace / 运行期记录解释 patch 的意义
5. 输出：
   - patch
   - records
   - reports
   - review bundle
   - share script

## 两阶段推进

### 阶段 A：真实 patch
- patch 点选择
- before baseline
- 真实 patch
- after 结果
- stats diff / review note

### 阶段 B：trace 与运行期证据收口
- 补关键 trace / runtime 证据
- 把 patch 前后现象和源码路径对上
- 收成总报告与分享稿

## 最后会交付什么

- `patches/0001-virtio_net-*.patch`
- `records/<ts>/before/`
- `records/<ts>/after/`
- `records/<ts>/trace/`
- `reports/final_project_report.md`
- `reports/patch_trace_mapping.md`
- `reports/review_bundle.md`
