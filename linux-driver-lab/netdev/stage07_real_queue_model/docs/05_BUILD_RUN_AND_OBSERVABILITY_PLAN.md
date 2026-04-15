# 05_BUILD_RUN_AND_OBSERVABILITY_PLAN

## 目标

stage07 不能只写代码，还必须从一开始就考虑：

- 怎么 build
- 怎么 run
- 怎么 smoke
- 怎么观测
- 怎么生成报告

## scripts 规划

### `build.sh`
负责：
- 解析环境
- 编译 `driver/netdev_stage07.c`
- 生成 `output/netdev_stage07.ko`

### `run.sh`
负责：
- 加载模块
- 创建设备
- 设置最小测试环境

### `smoke.sh`
负责：
- 触发一轮最小 TX/RX
- 抓取 dmesg 与统计
- 判断通过/失败

### `stats_check.sh`
负责：
- 读取 debugfs / proc-like 统计
- 核对 index / queue / poll / irq 计数

### `trace_smoke.sh`
负责：
- 生成行为级 trace 样本
- 用于证明 queue lifecycle 发生顺序正确

## 建议统计项

至少固定这些：
- tx_submit_count
- tx_complete_count
- rx_post_count
- rx_consume_count
- rx_refill_count
- irq_count
- napi_schedule_count
- napi_poll_count
- napi_budget_exhaust_count
- ring_full_count
- ring_empty_count
- drop_count

## 输出建议

### `output/`
放当前生成物：
- ko
- smoke log
- stats dump
- short report

### `records/`
放阶段证据：
- 某次 smoke 的完整日志
- 某次回归对比
- 某次 trace 样本

### `reports/`
放总结文档：
- acceptance
- stage04_vs_stage07
- virtio mapping review
