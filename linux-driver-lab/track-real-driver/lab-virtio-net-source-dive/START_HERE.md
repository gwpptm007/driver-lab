# START_HERE

## 这个 Lab 怎么开工

建议按 **三轮推进**。

### 第一轮：建立骨架
先只回答这些问题：

1. `virtio_net` 的核心结构体有哪些？
2. `probe/remove` 的主骨架是什么？
3. queue / napi / netdev ops 是怎么挂起来的？

对应文档：
- `docs/02_VIRTIO_NET_ARCHITECTURE.md`
- `docs/03_PROBE_TX_RX_READING_ORDER.md`

同时先跑：
- `scripts/collect_virtio_net_symbols.sh`
- `scripts/build_function_index.sh`

### 第二轮：看数据路径
先把主线搞清楚，不急着抠所有 helper。

- TX：`docs/04_TX_PATH.md`
- RX：`docs/05_RX_PATH.md`
- queue/NAPI/IRQ：`docs/06_QUEUE_NAPI_IRQ.md`

同时可跑：
- `scripts/extract_probe_path.sh`
- `scripts/extract_tx_path.sh`
- `scripts/extract_rx_path.sh`

### 第三轮：控制面与映射
- `docs/07_FEATURES_ETHTOOL_XDP.md`
- `docs/08_STAGE_TO_VIRTIO_NET_MAPPING.md`
- `docs/09_NEXT_STEP_PATCH_POINTS.md`

同时：
- 更新 `reports/stage_vs_virtio_net_report.md`
- 跑 `scripts/trace_virtio_net_basic.sh`

## 推荐节奏

### Day 1
- 架构 + probe 骨架

### Day 2
- TX 路径

### Day 3
- RX 路径

### Day 4
- queue / NAPI / IRQ

### Day 5
- features / ethtool / XDP

### Day 6
- stage 映射 + next-step patch points + 验收总结


## 分轮次开工建议

### Round1
```bash
./scripts/run_round1_arch_scan.sh
```

### Round2
```bash
./scripts/run_round2_txrx_scan.sh
```

### Round3
```bash
./scripts/run_round3_feature_scan.sh
```


## 看样例时的顺序

1. `docs/18_SAMPLE_OUTPUTS_USAGE.md`
2. `records/examples/2026-demo-round1-arch/`
3. `records/examples/2026-demo-round2-txrx/`
4. `records/examples/2026-demo-round3-feature-xdp/`
5. `reports/virtio_net_round_progress_board.md`


## Round1 正文建议先看这些

1. `docs/19_ROUND1_ARCH_ANALYSIS_DRAFT.md`
2. `docs/20_ROUND1_PROBE_READING_NOTES.md`
3. `reports/round1_arch_summary_draft.md`
4. `reports/virtio_net_round1_key_questions.md`


## Round2 正文建议先看这些

1. `docs/21_ROUND2_TX_ANALYSIS_DRAFT.md`
2. `docs/22_ROUND2_RX_ANALYSIS_DRAFT.md`
3. `reports/round2_txrx_summary_draft.md`
4. `reports/virtio_net_round2_key_questions.md`
5. `reports/virtio_net_txrx_path_seed.md`


## Round3 正文建议先看这些

1. `docs/23_ROUND3_QUEUE_NAPI_IRQ_ANALYSIS_DRAFT.md`
2. `docs/24_ROUND3_FEATURE_OFFLOAD_XDP_ANALYSIS_DRAFT.md`
3. `reports/round3_queue_feature_summary_draft.md`
4. `reports/virtio_net_round3_key_questions.md`
5. `reports/virtio_net_patch_candidates_seed.md`


## 最终收口建议先看这些

1. `docs/25_FINAL_SYNTHESIS_REPORT.md`
2. `docs/26_SHARE_DECK_SCRIPT.md`
3. `docs/27_PATCH_TRACING_PRIORITY_PLAN.md`
4. `reports/virtio_net_final_review_bundle.md`
5. `reports/virtio_net_maturity_assessment.md`
6. `reports/virtio_net_next_step_exec_board.md`
