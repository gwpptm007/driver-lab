# stage07_real_queue_model

> stage07 的目标，是把"教学型 ring + NAPI + DMA"推进到"准真实队列驱动模型"。

## 本阶段要解决什么

stage04 已经把 ring / DMA / refill 做出来了，但它的 queue 模型仍然偏教学。

stage07 在此基础上引入：
- **6 个显式 index**：`submit_idx / notify_idx / complete_idx / post_idx / device_idx / consume_idx`
- **4 状态 slot**：`FREE / POSTED / SUBMITTED / DONE`
- **更清晰的 producer / consumer 边界**
- **与 `virtio-net` 的结构映射**

## 核心文档

- [START_HERE.md](START_HERE.md) — 阅读顺序和快速开始
- [docs/01_STAGE_OVERVIEW.md](docs/01_STAGE_OVERVIEW.md) — 目标与模型
- [docs/02_USER_GUIDE.md](docs/02_USER_GUIDE.md) — 使用指南
- [docs/03_ACCEPTANCE.md](docs/03_ACCEPTANCE.md) — 验收标准
- [docs/04_DEEP_LEARNING.md](docs/04_DEEP_LEARNING.md) — 深度分析

## 推荐先看

```bash
cat START_HERE.md
```
