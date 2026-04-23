# lab-virtio-net-source-dive

> 所属：`track-real-driver/`

## 一句话定位

这不是“再写一个新驱动”，而是把你已经完成的 `netdev/stage00~stage14` 作为知识基线，切换到真实 Linux `virtio_net` 驱动源码专题分析。

## 这个 Lab 最终要交付什么

1. `virtio_net` 的整体结构化文档
2. TX/RX/NAPI/queue/interrupt 的主路径整理
3. `stage00~stage14 ↔ virtio_net` 的映射报告
4. 用于辅助阅读的最小脚本与记录模板
5. 下一步小 patch / trace / 观测实验的切入点

## 为什么第一个选 virtio_net

- 你已经做过 `net_device` / `skb` / NAPI / ring / multi-queue / MSI-X / page_pool / ethtool / offload / XDP
- `virtio_net` 正好能把这些知识放回真实驱动里重新观察
- 它比一上来就读体量更大的物理 NIC 更适合作为真实驱动第一站

## 本 Lab 的工作方式

### 不是
- 不是直接写新的 `.ko`
- 不是先并行读很多网卡驱动
- 不是一开始就展开完整 host/vhost/QEMU 后端

### 而是
- 先读 `virtio_net` 自身
- 先搞清 probe / queue / TX / RX / NAPI / feature / XDP
- 再把这些点映射回你已经完成的 `stage00~stage14`

## 阅读顺序

1. `START_HERE.md`
2. `docs/01_LAB_OVERVIEW.md`
3. `docs/02_VIRTIO_NET_ARCHITECTURE.md`
4. `docs/03_PROBE_TX_RX_READING_ORDER.md`
5. `docs/04_TX_PATH.md`
6. `docs/05_RX_PATH.md`
7. `docs/06_QUEUE_NAPI_IRQ.md`
8. `docs/07_FEATURES_ETHTOOL_XDP.md`
9. `docs/08_STAGE_TO_VIRTIO_NET_MAPPING.md`
10. `docs/09_NEXT_STEP_PATCH_POINTS.md`
11. `docs/10_ACCEPTANCE_AND_NEXT_STEP.md`


## 推荐推进节奏

建议按 3 轮推进，而不是一次把整个 `virtio_net.c` 全部啃完：

### Round1：架构与 probe
- 先搞清驱动骨架、私有结构体、queue / napi / netdev 的关系
- 对应文档：`docs/11_ROUND1_ARCH_CHECKLIST.md`

### Round2：TX / RX 主路径
- 重点建立自己的路径图
- 对应文档：`docs/12_ROUND2_TXRX_CHECKLIST.md`

### Round3：feature / ethtool / XDP
- 把 `stage12~stage14` 映射回真实驱动
- 对应文档：`docs/13_ROUND3_FEATURE_XDP_CHECKLIST.md`


## 当前仓库里已经附带的示范资源

为了让这个 Lab 不停留在“只有框架”，当前包里额外提供了：

- `records/examples/`：Round1 / Round2 / Round3 的示范性记录目录
- `docs/16_MASTER_EXECUTION_CHECKLIST.md`：总执行清单
- `docs/17_DELIVERABLES_FOR_REVIEW.md`：评审交付清单
- `docs/18_SAMPLE_OUTPUTS_USAGE.md`：样例使用方式说明
- `reports/virtio_net_round_progress_board.md`：进度看板
- `reports/virtio_net_first_pass_sample.md`：第一轮总结样例


## 当前已经补到哪一层

截至当前版本，这个 Lab 不再只有“计划、模板、样例”，还新增了：

- `docs/19_ROUND1_ARCH_ANALYSIS_DRAFT.md`
- `docs/20_ROUND1_PROBE_READING_NOTES.md`
- `reports/round1_arch_summary_draft.md`
- `reports/virtio_net_round1_key_questions.md`

也就是说，已经开始进入 **Round1 正文初稿** 阶段。


## 当前已经补到 Round2

截至当前版本，`lab-virtio-net-source-dive` 已经新增：

- `docs/21_ROUND2_TX_ANALYSIS_DRAFT.md`
- `docs/22_ROUND2_RX_ANALYSIS_DRAFT.md`
- `reports/round2_txrx_summary_draft.md`
- `reports/virtio_net_round2_key_questions.md`
- `reports/virtio_net_txrx_path_seed.md`

也就是说，这个 Lab 已经从：
- Round1 架构 / probe
推进到了：
- Round2 TX / RX 主路径初稿


## 当前已经补到 Round3

截至当前版本，`lab-virtio-net-source-dive` 已经新增：

- `docs/23_ROUND3_QUEUE_NAPI_IRQ_ANALYSIS_DRAFT.md`
- `docs/24_ROUND3_FEATURE_OFFLOAD_XDP_ANALYSIS_DRAFT.md`
- `reports/round3_queue_feature_summary_draft.md`
- `reports/virtio_net_round3_key_questions.md`
- `reports/virtio_net_patch_candidates_seed.md`

也就是说，这个 Lab 已经从：
- Round1 架构 / probe
- Round2 TX / RX 主路径
推进到了：
- Round3 事件推进模型 + 能力边界模型


## 当前已经收成最终可评审版

截至当前版本，`lab-virtio-net-source-dive` 已经新增：

- `docs/25_FINAL_SYNTHESIS_REPORT.md`
- `docs/26_SHARE_DECK_SCRIPT.md`
- `docs/27_PATCH_TRACING_PRIORITY_PLAN.md`
- `reports/virtio_net_final_review_bundle.md`
- `reports/virtio_net_maturity_assessment.md`
- `reports/virtio_net_next_step_exec_board.md`

这意味着这条 Lab 已经从：
- 分轮正文
推进到了：
- 总收口 / 分享稿 / 后续执行优先级
