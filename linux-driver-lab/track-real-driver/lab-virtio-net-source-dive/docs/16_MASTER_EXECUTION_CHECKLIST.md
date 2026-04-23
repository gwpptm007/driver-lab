# 16_MASTER_EXECUTION_CHECKLIST

> 这是一份“整包执行总清单”。你真正推进 `lab-virtio-net-source-dive` 时，建议每完成一项就打勾，而不是散着做。

## Phase A：准备

- [ ] 确认内核源码目录可用（默认：`/home/wq7/workspace/kernel-src/linux-5.15.10`）
- [ ] 进入 `track-real-driver/lab-virtio-net-source-dive/`
- [ ] 阅读 `README.md`
- [ ] 阅读 `START_HERE.md`

## Phase B：Round1（架构 / probe）

- [ ] 运行 `./scripts/run_round1_arch_scan.sh`
- [ ] 生成人工阅读目录 `records/<ts>-round1-arch/`
- [ ] 完成 `docs/02_VIRTIO_NET_ARCHITECTURE.md` 第一版
- [ ] 完成 `docs/03_PROBE_TX_RX_READING_ORDER.md` 第一版
- [ ] 至少写 2 份 `FUNCTION_NOTE`
- [ ] 回填一版 `reports/virtio_net_map.md`

## Phase C：Round2（TX / RX）

- [ ] 运行 `./scripts/run_round2_txrx_scan.sh`
- [ ] 补 `docs/04_TX_PATH.md`
- [ ] 补 `docs/05_RX_PATH.md`
- [ ] 至少留 1 份 trace 记录
- [ ] 回填 `reports/stage_vs_virtio_net_report.md`

## Phase D：Round3（feature / ethtool / XDP）

- [ ] 运行 `./scripts/run_round3_feature_scan.sh`
- [ ] 补 `docs/07_FEATURES_ETHTOOL_XDP.md`
- [ ] 完成 `stage12~stage14` 映射
- [ ] 写出下一步 patch / trace 候选点

## Phase E：收口

- [ ] 完成一份总总结
- [ ] 完成一份面试 / 分享版提纲
- [ ] 产出可评审的 `records/` 与 `reports/`
