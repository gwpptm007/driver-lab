# round1_arch_summary_draft

## 目的

这是一份面向评审的 Round1 汇总草稿，用来回答：

- 第一轮到底看了什么
- 第一轮看完以后，后续读 TX/RX 会不会更稳
- 这份 Lab 是否已经从“计划”推进成“真正可写分析”的状态

## 当前判断

当前 `lab-virtio-net-source-dive` 已经具备了 Round1 的基本推进条件：

- 有入口文档
- 有执行脚本
- 有记录模板
- 有示范性 records / reports
- 有 Round1 正文初稿

## Round1 的核心收获

### 1. 建立了正确的阅读层次
先分：
- 设备模型层
- netdev 接入层
- queue/NAPI 组织层

再进入 TX/RX。

### 2. 明确了 `virtnet_info` 的锚点作用
它是后续理解 queue、feature、poll 的中心。

### 3. 纠正了对 probe 的狭义理解
probe 不只是“注册网卡”，还承担能力协商、资源建立、生命周期骨架建立。

## 下一步建议

直接进入 Round2：

- TX path
- RX path
- queue / callback / poll / notify 的协同

并在这一轮开始形成真正的路径图。
