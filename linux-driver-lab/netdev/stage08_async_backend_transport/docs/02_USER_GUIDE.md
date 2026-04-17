# USER_GUIDE

## 快速开始

### 阅读顺序

1. `STAGE_OVERVIEW.md` — 先理解目标与模型
2. `driver/netdev_stage08.c` — 看代码实现
3. 本文档 — 掌握使用方式

### 测试流程

```bash
# 1. 编译 + 运行
./scripts/build.sh
./scripts/run.sh reload

# 2. smoke 测试
./scripts/smoke.sh

# 3. 观测验证
./scripts/stats_check.sh
./scripts/timeline_check.sh
```

### 关注指标

| 指标 | 含义 |
|------|------|
| `doorbell_count` | doorbell 触发次数 |
| `backend_schedule_count` | backend 入队次数 |
| `backend_run_count` | backend 实际运行次数 |
| `backend_tx_processed` | backend 处理的 TX 帧数 |
| `backend_rx_produced` | backend 生产的 RX 帧数 |
| `irq_count` | irq 触发次数 |
| `napi_poll_count` | NAPI poll 次数 |
| `tx_complete_count` | TX 完成次数 |
| `rx_consume_count` | RX 消费次数 |

---

## 模块参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `ifname` | `nds8` | 设备名 |
| `ring_size` | `128` | TX/RX 队列深度 |
| `napi_weight` | `32` | NAPI poll weight |
| `rx_buf_size` | `2048` | RX buffer 大小 |
| `backend_delay_us` | `0` | 人工 backend 延迟（模拟真实设备） |
| `backend_batch` | `32` | backend 每次最大处理帧数 |

### 示例：注入人工延迟观察异步效果

```bash
# 加载时指定 backend_delay_us
insmod netdev_stage08.ko backend_delay_us=100
```

---

## debugfs 观测出口

```
/sys/kernel/debug/netdev_stage08/
├── stats      # 全部原子计数
├── queues     # 队列 index + slot state dump
└── timeline   # 8 个时间戳 + 4 个 delta
```

### timeline 关键 delta

- `delta_submit_to_doorbell_ns` — submit 到 doorbell 的延迟
- `delta_doorbell_to_backend_ns` — doorbell 到 backend 执行的延迟
- `delta_backend_to_irq_ns` — backend 完成到 irq 的延迟
- `delta_irq_to_poll_ns` — irq 到 NAPI poll 的延迟

---

## 阶段计划

1. 建目录
2. 写目标与边界
3. 落第一版异步 backend driver
4. 补 build/run/smoke/timeline 脚本
5. 测试机上做第一轮验证
6. 基于实测收口
