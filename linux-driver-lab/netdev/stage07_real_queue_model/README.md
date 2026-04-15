# stage07_real_queue_model

## 阶段定位

`stage07_real_queue_model` 是 netdev 第二阶段在 `stage06_arm64_migration` 之后的下一跳。

它不是一下子做成“真实网卡驱动”，而是把当前 `stage04_ring_dma` 的教学型驱动模型，推进成：

- 更清晰的 queue 生命周期
- 更明确的 producer / consumer index
- 更接近真实驱动的 submit / notify / complete / refill 关系
- 更容易和 `virtio-net` 建立结构映射

一句话说：

> stage07 的目标，是把“教学型 ring + NAPI + DMA”推进到“准真实队列驱动模型”。

## 本次已落地内容（v1）

这次不再只是 scaffold，已经落下第一版核心代码：

- `driver/netdev_stage07.c`
  - 单 TX queue + 单 RX queue + 单 NAPI
  - `submit_idx / notify_idx / complete_idx`
  - `post_idx / device_idx / consume_idx`
  - `stage07_kick_device()` 明确承担 notify/backend 角色
  - `stage07_poll()` 明确承担 TX complete + RX consume/refill
- `include/netdev_stage07_compat.h`
  - 收敛 `netif_napi_add` 兼容宏
- `tools/`
  - `send_stage07_frame.c`
  - `recv_stage07_frame.c`
- `scripts/`
  - `build.sh`
  - `run.sh`
  - `smoke.sh`
  - `stats_check.sh`
  - `trace_smoke.sh`

## 当前版本边界

当前 v1 仍然是教学型伪设备：

- backend 仍用 `memcpy` 模拟 device DMA copy
- 不做多队列
- 不做 RSS / offload / XDP
- 不追求极限吞吐

但它已经把 stage07 最关键的队列边界落下来了。

## 建议阅读顺序

1. `START_HERE.md`
2. `docs/01_STAGE_GOAL_AND_BOUNDARY.md`
3. `docs/02_QUEUE_MODEL_AND_DATA_STRUCTURES.md`
4. `docs/03_NOTIFY_IRQ_NAPI_COMPLETION.md`
5. `docs/04_VIRTIO_MAPPING.md`
6. `docs/06_ACCEPTANCE_AND_MILESTONES.md`
7. `driver/netdev_stage07.c`
8. `TASKS.md`
