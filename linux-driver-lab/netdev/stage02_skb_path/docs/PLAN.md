# stage02_skb_path / PLAN

## 阶段目标
围绕 `skb` 建立软件 TX/RX 闭环，并用教学型 software loopback 把 sender / driver / receiver 串起来。

## 关键设计
- TX 入口：`ndo_start_xmit()`
- RX 注入：`netif_rx()`
- 构包模式：`loop_mode=copy|clone`
- 观测口径：`ip -s link` + debugfs + userspace receiver

## 本阶段不做
- 不做 NAPI
- 不做 ring / descriptor
- 不做 DMA
- 不做 RX replenishment

## 推荐阅读
1. `01_STAGE_GOAL_AND_BOUNDARY.md`
2. `02_SKB_LIFECYCLE_AND_DESIGN.md`
3. `03_SOFTWARE_LOOPBACK_PATH.md`
4. `04_TEST_AND_ACCEPTANCE.md`

## 验收口径
见 `../../docs/06_ACCEPTANCE_AND_MILESTONES.md` 中 M2。
