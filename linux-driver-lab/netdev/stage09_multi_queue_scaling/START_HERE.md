# stage09 / START HERE

## 学习顺序建议

**第一次接触 stage09，按以下顺序阅读**：

1. [README.md](README.md) — 项目定位和整体架构（5 分钟）
2. [docs/01_STAGE_OVERVIEW.md](docs/01_STAGE_OVERVIEW.md) — 为什么要多队列、与 stage08 的区别
3. [docs/02_QUEUE_MODEL.md](docs/02_QUEUE_MODEL.md) — 每队列的数据结构（TX/RX ring、timeline、stats）
4. [docs/03_FLOW_DISTRIBUTION_AND_NAPI.md](docs/03_FLOW_DISTRIBUTION_AND_NAPI.md) — 队列选择和 per-queue NAPI
5. [docs/04_DRIVER_LAYOUT.md](docs/04_DRIVER_LAYOUT.md) — struct 布局和 init/exit 流程
6. [docs/05_ACCEPTANCE.md](docs/05_ACCEPTANCE.md) — 通过标准和验证方法

**选读**：
- [docs/06_DEEP_LEARNING.md](docs/06_DEEP_LEARNING.md) — queue affinity、下一步方向

---

## 第一轮执行步骤

### 1. 构建

```bash
cd linux-driver-lab/netdev/stage09_multi_queue_scaling
./scripts/build.sh
```

### 2. 启动 QEMU 并加载

```bash
./scripts/run.sh reload
```

### 3. 运行 smoke test

```bash
./scripts/smoke.sh
```

### 4. 验证多队列分布

```bash
./scripts/queue_dist_check.sh records/<latest>
```

### 5. 验证异步链路

```bash
./scripts/timeline_check.sh records/<latest>
```

### 6. 查看各队列 timeline

```bash
cat /sys/kernel/debug/netdev_stage09/timeline
```

---

## 调试入口

### 查看 dmesg

```bash
dmesg | grep -E 'stage09|netdev_stage09' | tail -n 200
```

### 收集 trace log

```bash
./scripts/trace_smoke.sh
```

### 查看 per-queue stats

```bash
cat /sys/kernel/debug/netdev_stage09/stats
```

### 查看 per-queue ring 状态

```bash
cat /sys/kernel/debug/netdev_stage09/queues
```

---

## 文件速查

| 文件 | 说明 |
|------|------|
| `driver/netdev_stage09.c` | 驱动源码（含所有【学习】注释） |
| `include/netdev_stage09_compat.h` | 内核版本兼容宏 |
| `tools/send_stage09_frame.c` | TX 测试工具 |
| `tools/recv_stage09_frame.c` | RX 测试工具 |
| `scripts/build.sh` | 编译 driver + tools |
| `scripts/run.sh` | 启动 QEMU 并加载驱动 |
| `scripts/smoke.sh` | 冒烟测试 |
| `scripts/queue_dist_check.sh` | 多队列分布验证 |
| `scripts/timeline_check.sh` | 异步链路验证 |
| `scripts/trace_smoke.sh` | dmesg 日志收集 |
