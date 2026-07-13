# TEST_RECORD_20260711

## 1. 目标

创建 `project-rdma-performance-tuning` 第一版，验证 SEND completion latency 基线可以编译、运行、输出统计。

## 2. 测试命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
SUDO_PASSWORD='wq123456!' make test
```

## 3. 预期结果

```text
PASS: RDMA SEND latency smoke test
```

## 4. 实测结果

135 执行结果：

```text
script_config name=perf_smoke_test device=rxe0 netdev=ens34 gid_addr=fe80::34 gid_index=1 iterations=100
script_step=prepare_rxe status=start netdev=ens34
script_step=prepare_rxe status=done
script_case=send_latency status=start server_log=tests/perf-server.log client_log=tests/perf-client.log
PASS: RDMA SEND latency smoke test
script_case=send_latency status=pass
```

server 关键日志：

```text
app_config role=server mode=perf-send-latency listen=127.0.0.1 server=127.0.0.1 port=18600 device=rxe0 ib_port=1 gid_index=1 flags=
perf_config role=server test=send_latency iterations=100
TCP_CONTROL_PLANE_PASS
RC_QP_RTS_PASS
server_perf_recv_cqe iteration=0 cqe_wr_id=200000 status=success opcode=128 byte_len=15
server_perf_recv_cqe iteration=99 cqe_wr_id=200099 status=success opcode=128 byte_len=15
PERF_SEND_LATENCY_SERVER_PASS
cleanup=complete result=pass
```

client 关键日志：

```text
app_config role=client mode=perf-send-latency listen=127.0.0.1 server=127.0.0.1 port=18600 device=rxe0 ib_port=1 gid_index=1 flags=
perf_config role=client test=send_latency iterations=100
TCP_CONTROL_PLANE_PASS
RC_QP_RTS_PASS
client_perf_send_cqe iteration=0 cqe_wr_id=100000 status=success opcode=0 byte_len=15
client_perf_send_cqe iteration=99 cqe_wr_id=100099 status=success opcode=0 byte_len=15
perf_result test=send_latency iterations=100 avg_ns=7450 min_ns=5009 p50_ns=5681 p95_ns=11801 p99_ns=46625 max_ns=57194
PERF_SEND_LATENCY_CLIENT_PASS
cleanup=complete result=pass
```

## 5. 指标含义

```text
send_completion_latency = client post_send 前时间戳 到 client SEND CQE 出现后的时间戳
```

这不是业务 RTT，也不是硬件 RNIC latency。

## 6. batch WR 阶段更新

目标：在已完成的 single SEND completion latency 基线之后，追加 batch SEND 对比。

实现点：

- server 先保留 single SEND/RECV 循环，随后进入 batch phase。
- batch phase 中 server 一次链式 post N 个 RECV WR。
- client 一次链式 `ibv_post_send()` 提交 N 个 SEND WR。
- client poll N 个 SEND CQE 后记录 `avg_batch_ns`、`avg_msg_ns`、`msg_per_sec`。
- server poll N 个 RECV CQE，并逐条校验 payload。

新增可配置项：

```bash
PERF_BATCH_SIZE=8
```

新增关键 marker：

```text
PERF_BATCH_SEND_SERVER_PASS
PERF_BATCH_SEND_CLIENT_PASS
perf_result test=batch_send
perf_throughput test=batch_send
perf_compare single_vs_batch
```

## 7. 本地开发验证记录

当前 IDE 所在 Windows PowerShell 环境没有 `bash`、`make`、`gcc`，且 `wsl.exe` 可用但未安装 Linux 发行版。因此本机无法完成 RDMA/Linux 编译和运行。

已尝试命令：

```powershell
bash tests/perf_smoke_test.sh
```

结果：

```text
bash : The term 'bash' is not recognized as the name of a cmdlet...
```

已尝试命令：

```powershell
wsl bash -lc "cd /mnt/e/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning && make clean && make"
```

结果：WSL 提示需要先安装 Linux 发行版。

因此本轮本地只完成代码静态检查和 marker 检查，完整编译/运行放到 135。

## 8. 135 小样本验证命令

开发阶段先跑小样本，确认协议和 marker：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
SUDO_PASSWORD='wq123456!' make quickcheck
```

预期：

```text
script_config name=perf_smoke_test ... iterations=16 batch_size=4
script_case=send_latency_batch_wr status=start ...
PASS: RDMA SEND latency + batch WR smoke test
script_case=send_latency_batch_wr status=pass
```

只看关键 marker：

```bash
make markers
```

预期包含：

```text
tests/perf-server.log:PERF_SEND_LATENCY_SERVER_PASS
tests/perf-server.log:PERF_BATCH_SEND_SERVER_PASS
tests/perf-server.log:cleanup=complete result=pass
tests/perf-client.log:perf_result test=send_latency ...
tests/perf-client.log:perf_result test=batch_send ...
tests/perf-client.log:perf_throughput test=batch_send ...
tests/perf-client.log:perf_compare single_vs_batch ...
tests/perf-client.log:PERF_SEND_LATENCY_CLIENT_PASS
tests/perf-client.log:PERF_BATCH_SEND_CLIENT_PASS
tests/perf-client.log:cleanup=complete result=pass
```

## 9. 135 阶段收口验证命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
SUDO_PASSWORD='wq123456!' make fullcheck
```

收口时记录以下输出：

```bash
make markers
```

需要关注：

- `send_latency avg_ns`：single SEND completion baseline。
- `batch_send avg_batch_ns`：每批 WR 完成耗时。
- `batch_send avg_msg_ns`：按消息数折算后的平均耗时。
- `msg_per_sec`：batch 阶段完成吞吐。
- `speedup_x100`：`single_avg_ns / batch_avg_msg_ns * 100`，例如 250 表示约 2.50x。

## 10. batch WR 继续加固记录

新增失败诊断：

- client `post_batch_send_failed batch=<n> count=<n> bad_wr_id=<id>`。
- server `post_batch_recv_failed batch=<n> count=<n> bad_wr_id=<id>`。
- 两端启动时检查 `batch_size * PERF_BATCH_SLOT_SIZE <= RDMA_CS_BUFFER_SIZE`，失败输出 `batch_buffer_too_small`。

本机继续验证：

```powershell
Get-Command bash,make,gcc,clang,cc -ErrorAction SilentlyContinue
```

结果：未找到可用本地 Linux 编译工具。

```powershell
wsl bash -lc "cd /mnt/e/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning && make clean && make"
```

结果：`wsl.exe` 提示需要先安装 Linux 发行版。

marker 检查命令：

```powershell
rg -n "batch_buffer_too_small|post_batch_send_failed|post_batch_recv_failed|PERF_BATCH_SEND|perf_compare single_vs_batch|PERF_BATCH_SIZE" linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
```

结果：上述诊断、配置项和 pass marker 均已落到代码、脚本、文档和测试记录中。

## 11. smoke test 数值断言

继续收紧 `make test` 通过条件：不仅要求 marker 存在，还要求关键统计值非 0。

新增断言：

```bash
grep -Eq 'perf_result test=send_latency .*avg_ns=[1-9][0-9]*' tests/perf-client.log
grep -Eq 'perf_result test=batch_send .*avg_msg_ns=[1-9][0-9]*' tests/perf-client.log
grep -Eq 'perf_throughput test=batch_send .*msg_per_sec=[1-9][0-9]*' tests/perf-client.log
```

目的：

- 避免程序只打印 PASS marker，但统计值异常为 0。
- 让 batch SEND 的 latency/throughput 对比具备最基本的数据有效性。

## 12. Makefile 入口继续收口

为减少 135 上手敲环境变量，`Makefile` 新增了可覆盖默认值：

```makefile
QUICK_ITERATIONS ?= 16
QUICK_BATCH_SIZE ?= 4
FULL_ITERATIONS ?= 1000
FULL_BATCH_SIZE ?= 8
```

并新增：

```bash
make help
```

可用于查看当前推荐入口和默认参数。

覆盖示例：

```bash
SUDO_PASSWORD='wq123456!' make quickcheck QUICK_ITERATIONS=32 QUICK_BATCH_SIZE=8
SUDO_PASSWORD='wq123456!' make fullcheck FULL_ITERATIONS=2000 FULL_BATCH_SIZE=8
```

## 13. 环境自检入口

为减少 135 上切环境、换内核后的人工排查，新增：

```bash
make envcheck
```

输出重点：

- `env_step=rdma_link`
- `env_step=ipv6_addr`
- `env_step=ibv_devinfo`
- `env_step=gid_window`

建议顺序：

```bash
SUDO_PASSWORD='wq123456!' make quickreport
```

阶段收口可直接：

```bash
SUDO_PASSWORD='wq123456!' make fullreport
```

## 14. CSV 导出入口

为后续整理 batch WR 对比数据，新增：

```bash
make report
make quickreport
make fullreport
```

输出文件：

```text
tests/perf-summary.csv
```

CSV 包含：

- single SEND `avg/min/p50/p95/p99/max`
- batch SEND `avg_batch_ns/avg_msg_ns/min/p50/p95/p99/max`
- batch `messages/total_ns/msg_per_sec`
- `speedup_x100`

## 15. batch size sweep 入口

为真正比较 batch WR 的收益区间，新增：

```bash
SUDO_PASSWORD='wq123456!' make sweep
```

覆盖示例：

```bash
SUDO_PASSWORD='wq123456!' make sweep SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make sweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

输出文件：

```text
tests/perf-sweep.csv
tests/perf-sweep-summary.md
tests/sweep/
```

CSV 每行对应一个 `batch_size`，包含该轮 single/batch/throughput/speedup 指标，便于后续画表或写总结。

`tests/sweep/` 会保留每个 `batch_size` 的：

- `batch-<n>-client.log`
- `batch-<n>-server.log`
- `batch-<n>-summary.csv`

## 16. inline data 阶段入口

inline 模式先作为与当前 batch 基线并行的实验入口，不替换现有 normal 路径。

推荐命令：

```bash
SUDO_PASSWORD='wq123456!' make inlinequickreport
SUDO_PASSWORD='wq123456!' make inlinefullreport
SUDO_PASSWORD='wq123456!' make inlinesweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
make comparesweeps
```

关键 marker：

```text
PERF_SEND_LATENCY_INLINE_SERVER_PASS
PERF_SEND_LATENCY_INLINE_CLIENT_PASS
PERF_BATCH_SEND_INLINE_SERVER_PASS
PERF_BATCH_SEND_INLINE_CLIENT_PASS
perf_result test=send_latency_inline
perf_result test=batch_send_inline
perf_compare single_vs_batch inline=on
```

`tests/perf-summary.csv` 追加了 `inline_mode` 列，用于区分 normal/inline 跑出来的结果。

## 17. normal vs inline sweep 对比

新增入口：

```bash
SUDO_PASSWORD='wq123456!' make inlinesweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
make comparesweeps
```

输出文件：

```text
tests/perf-sweep-inline.csv
tests/perf-sweep-inline-summary.md
tests/perf-inline-vs-normal-summary.md
tests/sweep-inline/
```

目标是把 normal 与 inline 两套 sweep 都沉淀成可比较的产物，而不是只看单次 inline quick/full report。

## 18. batch WR 阶段完整测试流程

建议按下面顺序执行，既符合“开发期小样本、收口期大样本”的节奏，也避免反复翻完整日志：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning

# 1) 先看环境，确认 RXE/IPv6/GID 都在
SUDO_PASSWORD='wq123456!' make envcheck

# 2) normal 小样本，先确认主路径和 marker
SUDO_PASSWORD='wq123456!' make quickreport
make markers

# 3) inline 小样本，确认 inline marker 和结果名切换
SUDO_PASSWORD='wq123456!' make inlinequickreport
make markers

# 4) 收口前做 normal 全量 sweep
SUDO_PASSWORD='wq123456!' make sweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"

# 5) 再做 inline 全量 sweep
SUDO_PASSWORD='wq123456!' make inlinesweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"

# 6) 汇总 normal vs inline 最优点位
make comparesweeps

# 7) 一键收口入口
SUDO_PASSWORD='wq123456!' make batchphaseclose SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

建议检查点：

- `make markers` 中是否同时看到 `PERF_SEND_LATENCY_*_PASS`、`PERF_BATCH_SEND_*_PASS`、`perf_result`、`perf_compare`。
- `tests/perf-sweep.csv` 与 `tests/perf-sweep-inline.csv` 是否都包含 `batch_size=1 2 4 8 16`。
- `tests/perf-inline-vs-normal-summary.md` 是否已经生成 best throughput / best latency / best speedup 三张对比表。
- `make batchphaseclose` 结尾是否出现 `compare_report_check=pass`。

若某轮失败，优先只看 marker 和 sweep 产物目录：

```bash
rg -n "PASS|fail|perf_result|perf_compare|inline=" tests/perf-client.log tests/perf-server.log
find tests/sweep tests/sweep-inline -maxdepth 1 -type f | sort
```

## 19. selective signaling 阶段入口

这一阶段的目标不是改消息数量，而是改 client 侧等待的 SEND CQE 数量。

推荐命令：

```bash
SUDO_PASSWORD='wq123456!' make selectivequickreport
SUDO_PASSWORD='wq123456!' make selectivefullreport
SUDO_PASSWORD='wq123456!' make selectivesweepreport SELECTIVE_SIGNAL_INTERVAL=4 SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
make compareselective
```

若要叠加 inline：

```bash
SUDO_PASSWORD='wq123456!' make selectiveinlinequickreport
SUDO_PASSWORD='wq123456!' make selectiveinlinesweepreport SELECTIVE_SIGNAL_INTERVAL=4 SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
make compareinlineselective

PERF_USE_INLINE=1 PERF_SIGNAL_INTERVAL=4 SUDO_PASSWORD='wq123456!' make quickreport
SWEEP_USE_INLINE=1 SWEEP_SIGNAL_INTERVAL=4 SUDO_PASSWORD='wq123456!' make sweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

关键 marker：

```text
PERF_BATCH_SEND_SELECTIVE_SERVER_PASS
PERF_BATCH_SEND_SELECTIVE_CLIENT_PASS
perf_result test=batch_send_selective
perf_compare single_vs_batch signal_mode=selective signal_interval=4
```

Selective sweep 产物会单独落到：

```text
tests/perf-sweep-sig4.csv
tests/perf-sweep-sig4-summary.md
tests/sweep-sig4/
tests/perf-sweep-inline-sig4.csv
tests/perf-sweep-inline-sig4-summary.md
tests/sweep-inline-sig4/
tests/perf-selective-vs-all-summary.md
tests/perf-inline-selective-vs-inline-summary.md
```

这样做的目的是把 normal / inline / selective / inline+selective 四类实验的证据分开保存，避免后跑的新模式覆盖前面的 batch 基线文件。

## 20. signal interval 矩阵收口验证（2026-07-12）

这一轮的目标是把 `PERF_SIGNAL_INTERVAL` 从“可配置项”推进到“有完整矩阵和结论的实验维度”。

### 20.1 先做隔离复现

上一次 `signalreport` 失败时，normal 与 inline 命令被串到了同一个远端 shell 里，日志现场不干净。先单独验证 `signal_interval=2`，确认不是实现缺陷：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
make clean
SUDO_PASSWORD='wq123456!' make selectivequickreport SELECTIVE_SIGNAL_INTERVAL=2
SUDO_PASSWORD='wq123456!' make selectivesweepreport SELECTIVE_SIGNAL_INTERVAL=2 SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

关键结果：

```text
PASS: RDMA SEND latency + batch WR smoke test inline=0 signal_interval=2
perf_result test=batch_send_selective ... signal_interval=2 signaled_total=8 avg_msg_ns=6449
perf_throughput test=batch_send_selective ... signal_interval=2 msg_per_sec=1550446
perf_compare single_vs_batch inline=off signal_mode=selective signal_interval=2 signaled_total=8 single_avg_ns=9728 batch_avg_msg_ns=6449 speedup_x100=150
sweep_summary=pass output=tests/perf-sweep-sig2-summary.md
```

结论：`interval=2` 本身可稳定通过，前一次失败属于远端 shell 串命令导致的现场污染，不是 batch/selective 逻辑错误。

### 20.2 normal signal interval 矩阵

命令：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
SUDO_PASSWORD='wq123456!' make signalreport SIGNAL_INTERVALS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

关键 marker：

```text
signal_interval_sweep_step status=done signal_interval=1
signal_interval_sweep_step status=done signal_interval=2
signal_interval_sweep_step status=done signal_interval=4
signal_interval_sweep_step status=done signal_interval=8
signal_interval_sweep_step status=done signal_interval=16
signal_interval_sweep=pass output=tests/perf-signal-interval-sweep.csv
signal_interval_summary=pass output=tests/perf-signal-interval-summary.md
signal_interval_check=pass matrix=tests/perf-signal-interval-sweep.csv summary=tests/perf-signal-interval-summary.md lines=6
```

summary 摘录：

```text
- best throughput interval: 16 (batch_size=16, msg_per_sec=258825)
- best latency interval: 16 (batch_size=16, batch_avg_msg_ns=3863)
- best speedup interval: 16 (batch_size=16, speedup_x100=168)
```

矩阵表：

```text
| 1  | 16 | 202077 | 16 | 4948 | 16 | 154 |
| 2  | 8  | 227792 | 8  | 4389 | 16 | 149 |
| 4  | 8  | 222016 | 8  | 4504 | 16 | 149 |
| 8  | 16 | 202461 | 16 | 4939 | 16 | 150 |
| 16 | 16 | 258825 | 16 | 3863 | 16 | 168 |
```

### 20.3 inline signal interval 矩阵

命令：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
SUDO_PASSWORD='wq123456!' make inlinesignalreport SIGNAL_INTERVALS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

关键 marker：

```text
signal_interval_sweep_step status=done signal_interval=1
signal_interval_sweep_step status=done signal_interval=2
signal_interval_sweep_step status=done signal_interval=4
signal_interval_sweep_step status=done signal_interval=8
signal_interval_sweep_step status=done signal_interval=16
signal_interval_sweep=pass output=tests/perf-signal-interval-inline-sweep.csv
signal_interval_summary=pass output=tests/perf-signal-interval-inline-summary.md
signal_interval_check=pass matrix=tests/perf-signal-interval-inline-sweep.csv summary=tests/perf-signal-interval-inline-summary.md lines=6
```

summary 摘录：

```text
- best throughput interval: 8 (batch_size=16, msg_per_sec=276909)
- best latency interval: 8 (batch_size=16, batch_avg_msg_ns=3611)
- best speedup interval: 8 (batch_size=16, speedup_x100=173)
```

矩阵表：

```text
| 1  | 16 | 208917 | 16 | 4786 | 16 | 148 |
| 2  | 16 | 207339 | 16 | 4823 | 8  | 140 |
| 4  | 16 | 248858 | 16 | 4018 | 16 | 152 |
| 8  | 16 | 276909 | 16 | 3611 | 16 | 173 |
| 16 | 16 | 249780 | 16 | 4003 | 16 | 159 |
```

### 20.4 本阶段完成结论

到 2026-07-12 为止，`project-rdma-performance-tuning` 当前范围内已经具备：

- single SEND completion latency baseline
- batch WR single vs batch
- inline data normal vs inline
- selective signaling vs all-signaled
- inline+selective vs inline
- signal interval 矩阵与 summary

后续若继续推进，建议单独开下一任务处理：

- 双机跨主机验证
- CPU affinity / NUMA / 调度影响

## 21. client-side CQ polling budget 收口验证（2026-07-12）

这一轮目标：不改协议与控制面，只把 client 侧 `ibv_poll_cq()` 每次最多取回多少个 CQE 做成一个独立实验维度。

### 21.1 实现边界

- 新增环境变量：`PERF_POLL_CQ_BUDGET=<N>`
- `N=1` 对应 `poll_mode=single`
- `N>1` 对应 `poll_mode=burst`
- 默认 `N=16`，保持现有 batch 路径兼容

### 21.2 quick 样本

命令：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
PERF_POLL_CQ_BUDGET=1 SUDO_PASSWORD='wq123456!' make quickreport
PERF_POLL_CQ_BUDGET=4 SUDO_PASSWORD='wq123456!' make quickreport
```

关键结果（budget=1）：

```text
script_config ... signal_interval=1 poll_budget=1
PASS: RDMA SEND latency + batch WR smoke test inline=0 signal_interval=1 poll_budget=1
perf_config role=client test=send_latency iterations=16 inline=off poll_mode=single poll_budget=1
perf_config role=client test=batch_send iterations=16 batch_size=4 batches=4 inline=off signal_mode=all signal_interval=1 poll_mode=single poll_budget=1
perf_compare single_vs_batch inline=off signal_mode=all signal_interval=1 signaled_total=16 poll_mode=single poll_budget=1 ...
```

并确认：

```text
tests/perf-summary.csv
1:26
2:26
```

说明 CSV 已从 24 列扩到 26 列，新增 `poll_mode,poll_budget`。

### 21.3 normal poll budget 矩阵

命令：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
SUDO_PASSWORD='wq123456!' make pollreport POLL_CQ_BUDGETS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

关键 marker：

```text
poll_budget_sweep_step status=done poll_budget=1
poll_budget_sweep_step status=done poll_budget=2
poll_budget_sweep_step status=done poll_budget=4
poll_budget_sweep_step status=done poll_budget=8
poll_budget_sweep_step status=done poll_budget=16
poll_budget_sweep=pass output=tests/perf-poll-budget-sweep.csv
poll_budget_summary=pass output=tests/perf-poll-budget-summary.md
poll_budget_check=pass matrix=tests/perf-poll-budget-sweep.csv summary=tests/perf-poll-budget-summary.md lines=6
```

summary 摘录：

```text
- best throughput budget: 16 (batch_size=16, msg_per_sec=241605)
- best latency budget: 16 (batch_size=16, batch_avg_msg_ns=4138)
- best speedup budget: 1 (batch_size=16, speedup_x100=157)
```

矩阵表：

```text
| 1  | 4  | 230401 | 4  | 4340 | 16 | 157 |
| 2  | 16 | 200972 | 16 | 4975 | 16 | 148 |
| 4  | 16 | 241380 | 16 | 4142 | 8  | 142 |
| 8  | 16 | 230996 | 16 | 4329 | 16 | 152 |
| 16 | 16 | 241605 | 16 | 4138 | 16 | 156 |
```

### 21.4 inline poll budget 矩阵

命令：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
SUDO_PASSWORD='wq123456!' make inlinepollreport POLL_CQ_BUDGETS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

关键 marker：

```text
poll_budget_sweep_step status=done poll_budget=1
poll_budget_sweep_step status=done poll_budget=2
poll_budget_sweep_step status=done poll_budget=4
poll_budget_sweep_step status=done poll_budget=8
poll_budget_sweep_step status=done poll_budget=16
poll_budget_sweep=pass output=tests/perf-poll-budget-inline-sweep.csv
poll_budget_summary=pass output=tests/perf-poll-budget-inline-summary.md
poll_budget_check=pass matrix=tests/perf-poll-budget-inline-sweep.csv summary=tests/perf-poll-budget-inline-summary.md lines=6
```

summary 摘录：

```text
- best throughput budget: 1 (batch_size=16, msg_per_sec=275508)
- best latency budget: 1 (batch_size=16, batch_avg_msg_ns=3629)
- best speedup budget: 1 (batch_size=16, speedup_x100=170)
```

矩阵表：

```text
| 1  | 16 | 275508 | 16 | 3629 | 16 | 170 |
| 2  | 16 | 247155 | 16 | 4046 | 16 | 150 |
| 4  | 16 | 247742 | 16 | 4036 | 16 | 151 |
| 8  | 16 | 234298 | 16 | 4268 | 8  | 148 |
| 16 | 16 | 206422 | 16 | 4844 | 16 | 148 |
```

### 21.5 环境抖动说明

第一次跑 normal poll matrix 时，`poll_budget=8 batch_size=8` 这组在 `phase=resources_create` 早期失败，日志只显示：

```text
perf_config role=client test=batch_send ... poll_mode=burst poll_budget=8
phase=resources_create role=client status=start
cleanup=complete result=fail
```

随后对同一组做隔离复现：

```bash
PERF_POLL_CQ_BUDGET=8 PERF_BATCH_SIZE=8 PERF_ITERATIONS=1000 SUDO_PASSWORD='wq123456!' make test
```

结果 PASS，说明这次失败属于 RXE/环境瞬态抖动，而不是 poll budget 逻辑错误。第二次完整 `make pollreport` 已 fresh PASS。

### 21.6 本阶段状态更新

到 2026-07-12 为止，`project-rdma-performance-tuning` 当前范围内已经具备：

- single SEND completion latency baseline
- batch WR single vs batch
- inline data normal vs inline
- selective signaling vs all-signaled
- inline+selective vs inline
- signal interval 矩阵与 summary
- client-side CQ polling budget 矩阵与 summary

后续若继续推进，建议单独开下一任务处理：

- 双机跨主机验证
- CPU affinity / NUMA / 调度影响
