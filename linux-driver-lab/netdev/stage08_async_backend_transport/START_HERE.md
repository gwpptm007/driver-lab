# START_HERE — stage08_async_backend_transport

这是 stage08 的起点文档。

## 你先怎么理解 stage08

不要把它理解成"再做一个能发包的 netdev"。

要把它理解成：

- **stage07**：CPU 侧 queue lifecycle 已经讲透
- **stage08**：开始把 backend 当作一个独立执行体

也就是：

- 前端负责 submit / doorbell
- 后端 worker 负责异步消费 TX / 产出 RX / raise irq
- NAPI 负责批量 complete / consume / refill

## 当前建议阅读顺序

1. `docs/01_STAGE_OVERVIEW.md` — 目标、边界、前后端模型、时间线、数据结构
2. `docs/02_USER_GUIDE.md` — 使用方式、模块参数、debugfs 观测
3. `docs/03_ACCEPTANCE.md` — 验收标准和检查单
4. `driver/netdev_stage08.c` — 代码实现
5. `docs/04_DEEP_LEARNING.md` — 深度分析、调用链、与 virtio-net 映射

## 当前仓库内的实际定位

- `stage07_real_queue_model`：已经可视为上一阶段收尾成果
- `stage08_async_backend_transport`：现在开始成为新的主战场

## 本阶段当前最重要的输出

不是高吞吐，不是多队列，而是：

- 前后端边界
- doorbell 语义
- backend worker
- 异步 completion
- timeline 观测

## 快速测试

```bash
./scripts/build.sh
./scripts/run.sh reload
./scripts/smoke.sh
./scripts/stats_check.sh
./scripts/timeline_check.sh
```
