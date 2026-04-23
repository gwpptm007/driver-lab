# track-real-driver

> stage14 之后的第一条正式 Track：真实 Linux 驱动源码与补丁线。

## 定位

在 `netdev/stage00~stage14` 中，你已经完成了教学型 soft NIC 主线：

- `net_device`
- `sk_buff`
- NAPI
- ring / descriptor
- multi-queue
- MSI-X / per-queue IRQ
- page_pool
- ethtool / control plane
- offload
- XDP 入口

从这里继续往后推进，如果还沿用 `stage15 stage16 ...` 命名，会逐渐把“课程式推进”和“真实工业驱动研究”混在一起。

因此从 stage14 之后，正式切换为：

- `track`
- `lab`
- `project`

## 当前建议的第一个 Lab

- `lab-virtio-net-source-dive/`

它的职责，是把你已经写过的教学驱动与 Linux 内核中的真实 `virtio_net` 驱动建立一一映射。


## 本 Track 的推荐推进顺序

1. `lab-virtio-net-source-dive/`
2. `lab-real-driver-ethtool-stats/`
3. `lab-real-driver-small-patch/`

先把 `virtio_net` 看懂，再进入更偏“验证/补丁”的实验。


## 当前落地建议

不要把 `lab-virtio-net-source-dive` 只做成“放几篇文档”的静态目录，建议至少完成：

- Round1：架构 / probe / netdev / queue / napi
- Round2：TX / RX / trace
- Round3：feature / ethtool / XDP / mapping

这样它才算是一个真正的可交付 Lab。


## 当前这条 Track 的最小闭环

就当前阶段而言，`lab-virtio-net-source-dive` 至少要做到：

- 有 3 轮推进顺序
- 有辅助扫描脚本
- 有示范性 records / reports
- 有一份可用于评审/分享的总结提纲

做到这里，才算从“目录规划”推进成“可交付专题实验”。


## 当前已经从“计划型目录”推进到什么状态

`lab-virtio-net-source-dive` 现在已经完成了三层推进：

1. 目录与命名落地
2. 执行脚本 / 模板 / 样例落地
3. Round1 正文初稿落地

这意味着它已经不只是一个“以后再写”的空专题，而是可以真正持续沉淀阅读结果的真实 Lab。


## 当前已经进入主路径正文阶段

`lab-virtio-net-source-dive` 现在已经不只是：
- 目录
- 模板
- 样例
- Round1 架构正文

而是进一步进入了：
- Round2 TX 正文
- Round2 RX 正文
- TX/RX 汇总草稿

这说明它已经可以作为一个真正的源码专题持续推进，而不是停留在计划层。


## 当前已经接近一版完整专题闭环

`lab-virtio-net-source-dive` 现在已经包含：

- Round1：架构 / probe
- Round2：TX / RX 主路径
- Round3：queue/NAPI/IRQ 收口 + feature/offload/XDP 收口

从项目推进角度看，它已经不再只是“读源码计划”，而是接近一版真正可评审的专题闭环。


## 当前这条 Track 已经有了第一个可评审专题成品

`lab-virtio-net-source-dive` 现在已经具备：
- 主题定位
- 分轮正文
- 样例与模板
- 总报告
- 分享稿
- patch/tracing 后续优先级

它可以被视为 `track-real-driver` 的第一个成型专题。
