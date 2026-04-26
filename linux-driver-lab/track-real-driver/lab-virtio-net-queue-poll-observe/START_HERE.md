# START_HERE

## 推荐阅读顺序

1. `README.md`
2. `docs/01_GOAL_AND_SCOPE.md`
3. `docs/02_OBSERVE_CHAIN.md`
4. `docs/03_TRACE_PLAN.md`
5. `docs/04_WORKLOAD_AND_METRICS.md`
6. `docs/05_EXECUTION_FLOW.md`
7. `docs/06_ACCEPTANCE_AND_REVIEW.md`
8. `reports/queue_poll_exec_board.md`

## 建议开工顺序

### 第 1 步：确认正式环境
这个 Lab 仍然默认：
- QEMU guest
- `virtio_net` 接口
- 能跑 ping / iperf3

先确认：
```bash
ethtool -i <ifname>
```

### 第 2 步：创建本轮 records 目录
```bash
./scripts/bootstrap_record_dir.sh
```

### 第 3 步：先做 idle baseline
```bash
./scripts/run_idle_window.sh <ifname> <record-dir>
```

### 第 4 步：再做 ping workload
```bash
./scripts/run_ping_window.sh <ifname> <peer-ip> <record-dir>
```

### 第 5 步：最后再做 iperf3
```bash
./scripts/run_iperf_window.sh <ifname> <server-ip> <record-dir>
```

### 第 6 步：整理 trace 和结论
```bash
./scripts/summarize_deltas.sh <record-dir>
```
