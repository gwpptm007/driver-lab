# TEST_RECORD_20260713_PHASE5_TUNING

## 1. 目标

建立 burst、mempool cache、flow rule count 的可复现 p50/p99 对比矩阵，同时保证每个 case 的 packet/action 计数和 Phase 1-3 自测继续通过。

## 2. 环境

- 主机：`192.168.65.135`
- DPDK：`21.11.9`
- PMD：`net_pcap` + `net_null`
- EAL：`--no-pci --no-huge`
- lcores：`0-2`
- 每个 case：4096 packets / 4096 latency samples

## 3. 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/project-dpdk-flow-pipeline
chmod +x scripts/*.sh tests/*.sh tools/*.py
bash -n scripts/*.sh tests/*.sh
python3 -m py_compile tools/gen_flow_pcap.py tools/parse_tuning_matrix.py
make clean
make
make test-tuning
```

## 4. Case 验收

```text
FLOW_TUNING_CASE_PASS name=baseline burst=16 cache=250 extra_rules=0
FLOW_TUNING_CASE_PASS name=burst_1 burst=1 cache=250 extra_rules=0
FLOW_TUNING_CASE_PASS name=burst_32 burst=32 cache=250 extra_rules=0
FLOW_TUNING_CASE_PASS name=burst_64 burst=64 cache=250 extra_rules=0
FLOW_TUNING_CASE_PASS name=cache_0 burst=16 cache=0 extra_rules=0
FLOW_TUNING_CASE_PASS name=rules_64 burst=16 cache=250 extra_rules=61
FLOW_TUNING_CASE_PASS name=rules_512 burst=16 cache=250 extra_rules=509
FLOW_TUNING_PARSE_PASS cases=7
DPDK_FLOW_PIPELINE_PHASE5_TUNING_PASS
PASS: DPDK flow pipeline burst cache rule-count tuning matrix
script_summary name=tuning_matrix_test status=pass
```

## 5. 结果矩阵

| case | burst | cache | rules | p50 cycles | p99 cycles | p99 ns | vs baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 16 | 250 | 3 | 75 | 125 | 50 | 0% |
| burst_1 | 1 | 250 | 3 | 75 | 100 | 40 | -20% |
| burst_32 | 32 | 250 | 3 | 75 | 100 | 40 | -20% |
| burst_64 | 64 | 250 | 3 | 75 | 125 | 50 | 0% |
| cache_0 | 16 | 0 | 3 | 75 | 125 | 50 | 0% |
| rules_64 | 16 | 250 | 64 | 75 | 100 | 40 | -20% |
| rules_512 | 16 | 250 | 512 | 75 | 100 | 40 | -20% |

## 6. 结论与边界

7 个 case 全部通过，p50 均为 75 cycles，p99 位于 100-125 cycles。25 cycles 的差异不足以得出稳定性能排序；尤其不能解释为 512 条规则比 3 条规则更快。

当前证据证明调优变量、动态验收公式、矩阵执行和 CSV/Markdown 产物链路有效。结果仅覆盖 pcap PMD 下的 parse + software hash lookup + decision，不包含真实 NIC RX/TX、DMA、PCIe、RSS 和端到端吞吐。
