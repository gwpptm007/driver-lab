# START_HERE

## 推荐执行顺序

1. `README.md`
2. `docs/01_GOAL_AND_HYPOTHESIS.md`
3. `docs/02_TRACE_POINTS.md`
4. `docs/03_WORKLOAD_PLAN.md`
5. `docs/04_EXECUTION_FLOW.md`
6. `scripts/check_env.sh`
7. `scripts/run_idle_baseline.sh`
8. `scripts/run_ping_workload.sh`
9. `scripts/run_iperf_workload.sh`
10. `scripts/collect_logs.sh`
11. `docs/05_ACCEPTANCE.md`

## 一句话建议

先不要急着追求“全量 trace”，先拿到下面三类最小证据：

- idle baseline
- ping workload
- iperf3 workload

然后再把这些记录和 `source-dive` 里的 TX/RX/queue/NAPI 图对起来。
