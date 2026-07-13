# TEST_FLOW

## 1. 环境检查

```bash
pkg-config --modversion libdpdk
find /usr -name 'librte_net_pcap.so*' -print -quit
python3 --version
```

Phase 1 不依赖 Scapy，也不要求 hugepage；脚本使用 Python 标准库生成 pcap，并以 EAL `--no-huge` 运行功能 smoke。

## 2. 构建

```bash
cd linux-driver-lab/track-dpdk-advanced/project-dpdk-flow-pipeline
make clean
make
```

等价脚本：

```bash
bash scripts/01_build.sh
```

## 3. 自动 smoke

```bash
make test
```

执行顺序：

1. 清理 `tests/runtime` 并重新编译。
2. 生成 64 包、4 类五元组的 pcap。
3. 启动 `net_pcap0` 和 `net_null1` vdev。
4. 查询 queue/RSS capability，执行 `rte_flow_validate()`。
5. 装载 3 条 `rte_hash` 规则并进入 RX loop。
6. 验证 DROP/FORWARD/MARK/MISS 各 16 包。
7. 验证 per-rule stats、TX/free 守恒和 64 个 latency 样本。
8. 在同一个 `rte_hash` 上验证动态规则 add/update/delete 和 aging。
9. 启动两个 worker lcore，分别验证 shared 和 sharded table 模型。

## 4. 手工命令

```bash
python3 tools/gen_flow_pcap.py tests/runtime/flow_input.pcap 64

app/build/dpdk-flow-pipeline \
  -l 0-2 -n 4 --no-pci --no-huge \
  --file-prefix dpdk_flow_pipeline_manual \
  --vdev 'net_pcap0,rx_pcap=tests/runtime/flow_input.pcap' \
  --vdev 'net_null1' \
  -- --burst-size 16 --max-idle-polls 100000
```

## 5. Marker 检查

```bash
grep -E 'FLOW_(HASH|RULE_LOAD|PORT_CAPABILITY|RESULT|LATENCY)' \
  tests/runtime/flow_pipeline.log
grep -E 'RSS_MULTI|RTE_FLOW_|APP_RESULT|PHASE1_PASS|cleanup=' \
  tests/runtime/flow_pipeline.log
```

必须满足：

```text
FLOW_RULE_LOAD_PASS count=3
FLOW_RESULT hash_hits=48 hash_misses=16 rule_drop=16 forward=16 mark=16 default_drop=16 invalid=0
FLOW_LATENCY samples=64
APP_RESULT rx=64 tx=32 tx_failed=0 freed=32
DPDK_FLOW_PIPELINE_PHASE1_PASS
FLOW_RULE_ADD_UPDATE_DELETE_PASS
FLOW_RULE_AGING_PASS
DPDK_FLOW_PIPELINE_PHASE2_LIFECYCLE_PASS
FLOW_WORKER_RESULT mode=shared queue0=32 queue1=32 hits=48 misses=16 drop=16 forward=16 mark=16 default_drop=16
FLOW_WORKER_RESULT mode=sharded queue0=32 queue1=32 hits=48 misses=16 drop=16 forward=16 mark=16 default_drop=16
FLOW_WORKER_SHARED_TABLE_PASS
FLOW_WORKER_SHARDED_TABLE_PASS
DPDK_FLOW_PIPELINE_PHASE3_WORKER_PASS
cleanup=complete result=pass
```

`RSS_MULTI_QUEUE_BOUNDARY_BLOCKED` 和 `RTE_FLOW_BOUNDARY_BLOCKED` 是当前 pcap PMD 的预期能力边界，不是 software flow pipeline 失败。

Phase 4 的独立探测命令、返回值解释和真实 NIC 复验清单见 `TEST_RECORD_20260713_PHASE4_CAPABILITY_BOUNDARY.md`。

Phase 3 的两个 logical queue 由 main lcore 软件分流，确实运行在两个 worker lcore 上，但不能表述为 NIC RSS 或双硬件 RX queue PASS。

## 6. 135 验证命令

```bash
ssh -o BatchMode=yes wq7@192.168.65.135
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/project-dpdk-flow-pipeline
make test
```

## 7. Phase 5 调优矩阵

```bash
make test-tuning
```

脚本执行 7 个 case：

```text
baseline: burst=16 cache=250 rules=3
burst_1:  burst=1  cache=250 rules=3
burst_32: burst=32 cache=250 rules=3
burst_64: burst=64 cache=250 rules=3
cache_0:  burst=16 cache=0   rules=3
rules_64: burst=16 cache=250 rules=64
rules_512: burst=16 cache=250 rules=512
```

每个 case 必须满足：

```text
FLOW_RESULT hash_hits=3072 hash_misses=1024 rule_drop=1024 forward=1024 mark=1024 default_drop=1024 invalid=0
FLOW_LATENCY samples=4096
DPDK_FLOW_PIPELINE_PHASE3_WORKER_PASS
cleanup=complete result=pass
```

最终产物：

```text
tests/runtime/tuning/TUNING_MATRIX.csv
tests/runtime/tuning/TUNING_MATRIX.md
DPDK_FLOW_PIPELINE_PHASE5_TUNING_PASS
```

仓库保留的阶段结果位于 `tests/results/PHASE5_TUNING_MATRIX_20260713.*`。

## 8. Phase 6 错误边界

单独执行：

```bash
make test-boundary
```

| case | 注入条件 | 预期 marker |
|---|---|---|
| `expected_packets` | `--expected-packets 63` | `FLOW_CONFIG_BOUNDARY_REJECT reason=expected_packets` |
| `extra_rules` | `--extra-rules 1022` | `FLOW_CONFIG_BOUNDARY_REJECT reason=arguments` |
| `insufficient_ports` | 只创建一个 pcap vdev | `FLOW_PORT_BOUNDARY_REJECT available=1 required=2` |
| `insufficient_workers` | EAL 只启用 `-l 0-1` | `FLOW_WORKER_BLOCKED available=1 required=2` |

每个 case 都必须非零退出，并出现：

```text
cleanup=complete result=fail
FLOW_BOUNDARY_CASE_PASS
```

阶段 marker：

```text
DPDK_FLOW_PIPELINE_PHASE6_BOUNDARY_PASS
```

## 9. 完整收口回归

```bash
make clean
make
make test-all
```

`test-all` 顺序执行 64 包 smoke、7 case 调优矩阵和 4 case 错误边界，最终输出：

```text
DPDK_FLOW_PIPELINE_CURRENT_ENV_COMPLETE
```

完整执行记录见 `TEST_RECORD_20260713_PHASE6_CLOSEOUT.md`。
