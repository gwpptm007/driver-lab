# 17_DELIVERABLES_FOR_REVIEW

> 当你准备让别人评审这个 Lab 时，最少应该交哪些东西。

## 最低交付

1. `docs/02_VIRTIO_NET_ARCHITECTURE.md`
2. `docs/03_PROBE_TX_RX_READING_ORDER.md`
3. `docs/04_TX_PATH.md`
4. `docs/05_RX_PATH.md`
5. `docs/07_FEATURES_ETHTOOL_XDP.md`
6. `reports/stage_vs_virtio_net_report.md`
7. 一份 `records/<ts>-round1-arch/SUMMARY.md`
8. 一份 `records/<ts>-round2-txrx/SUMMARY.md`

## 标准交付

在最低交付基础上，再补：

- 一份 grouped function index
- 一份 trace 记录
- 一份“教学驱动 vs 真实驱动差异”总结
- 一份“下一步 patch/trace 候选点”总结

## 优秀交付

- 你能用 `docs/15_INTERVIEW_SHARE_OUTLINE.md` 做一场完整分享
- 你能指出后续哪个函数最适合做 small patch
- 你能解释为什么 stage14 之后切到 `track-real-driver`
