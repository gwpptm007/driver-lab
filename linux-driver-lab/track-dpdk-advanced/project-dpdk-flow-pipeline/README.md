# project-dpdk-flow-pipeline

本项目在已有 DPDK L3 forwarding 基础上增加可更新的精确流表、动作流水线、能力探测和决策尾延迟观测。当前状态：`DPDK_FLOW_PIPELINE_CURRENT_ENV_COMPLETE`。

## 当前已完成

- 从 Ethernet/IPv4/UDP 提取固定 16 字节五元组 key。
- 使用 `rte_hash` 装载和查询精确匹配规则。
- 支持 `DROP`、`FORWARD`、`MARK + FORWARD` 和 miss 默认丢弃。
- 输出 per-rule packet/byte 统计以及 hash hit/miss。
- 使用 TSC 采集 parse + hash lookup + decision 的 p50/p99/max。
- 查询 RX queue/RSS capability，并调用 `rte_flow_validate()` 探测硬件 offload 接口。
- 使用标准库生成 64 包 pcap，自动断言四类流量各 16 包。
- 动态规则支持 add、原位 action update、delete、generation 和 TSC aging。
- main lcore 通过两个 `rte_ring` 软件分流到两个 worker lcore。
- worker 分别验证 shared-readonly table 和 per-worker sharded table。
- 完成 burst/cache/rule-count 的 7 case、4096 包 p50/p99 对比矩阵。
- 完成参数、端口和 worker 资源不足的 4 类错误边界及清理验证。

135 的 pcap PMD 只有一个 RX queue，且不实现 `rte_flow`；因此 software pipeline PASS，RSS/multi-queue 和 hardware flow offload 记录为 boundary，不伪造硬件结果。

## 构建与测试

```bash
cd linux-driver-lab/track-dpdk-advanced/project-dpdk-flow-pipeline
make
make test
make test-all
```

关键结果：

```text
FLOW_RESULT hash_hits=48 hash_misses=16 rule_drop=16 forward=16 mark=16 default_drop=16 invalid=0
APP_RESULT rx=64 tx=32 tx_failed=0 freed=32
PASS_FLOW_HASH_ACTIONS
PASS_FLOW_LATENCY_SAMPLES
DPDK_FLOW_PIPELINE_PHASE1_PASS
FLOW_RULE_ADD_UPDATE_DELETE_PASS
FLOW_RULE_AGING_PASS
DPDK_FLOW_PIPELINE_PHASE2_LIFECYCLE_PASS
FLOW_WORKER_SHARED_TABLE_PASS
FLOW_WORKER_SHARDED_TABLE_PASS
DPDK_FLOW_PIPELINE_PHASE3_WORKER_PASS
DPDK_FLOW_PIPELINE_PHASE5_TUNING_PASS
DPDK_FLOW_PIPELINE_PHASE6_BOUNDARY_PASS
DPDK_FLOW_PIPELINE_CURRENT_ENV_COMPLETE
```

## 阶段状态

| Phase | 目标 | 当前状态 |
|---|---|---|
| 1 | `rte_hash` 精确匹配、动作、统计、p99、能力探测 | PASS |
| 2 | 规则 add/update/delete、generation 与 aging | PASS |
| 3 | queue-to-lcore worker、per-queue stats、shared/sharded table | PASS |
| 4 | 支持 PMD 上的 RSS/RETA 与 `rte_flow` create/query/destroy | `BOUNDARY_PCAP_RSS_RTE_FLOW` |
| 5 | burst/cache/rule-count/p50/p99 对比矩阵 | PASS |
| 6 | 故障边界、完整报告和项目收口 | PASS |

原理和 Mermaid/UML 见 `docs/ARCHITECTURE.md`，完整命令见 `tests/TEST_FLOW.md`。

Phase 5 单独执行：

```bash
make test-tuning
```

原始结果保存在 `tests/results/PHASE5_TUNING_MATRIX_20260713.csv` 和对应 Markdown 文件。当前差异只有 25 cycles，不能据此宣称某组参数在真实 NIC 上更优。

完整报告见 `reports/DPDK_FLOW_PIPELINE_FINAL_REPORT.md`。Phase 4 是待真实多队列 PMD 复验的硬件分支，不阻塞软件 pipeline 在当前环境收口。

Phase 4 独立边界证据见 `tests/TEST_RECORD_20260713_PHASE4_CAPABILITY_BOUNDARY.md`，Phase 6 全量收口记录见 `tests/TEST_RECORD_20260713_PHASE6_CLOSEOUT.md`。
