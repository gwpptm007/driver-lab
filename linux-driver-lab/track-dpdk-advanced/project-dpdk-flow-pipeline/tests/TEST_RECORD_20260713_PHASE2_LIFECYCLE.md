# TEST_RECORD_20260713_PHASE2_LIFECYCLE

## 1. 目标

在 Phase 1 software pipeline 不回归的前提下，验证动态规则 add、原位 action update、delete、generation 递增、inactive slot 复用和 TSC aging。

## 2. 环境与命令

- 主机：`192.168.65.135`
- DPDK：`21.11.9`
- PMD：`net_pcap` + `net_null`

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/project-dpdk-flow-pipeline
make clean
make
make test

grep -E 'FLOW_RULE_(ADD|AGING)|PHASE[12]|FLOW_RESULT|APP_RESULT' \
  tests/runtime/flow_pipeline.log
```

## 3. 关键证据

```text
FLOW_RESULT hash_hits=48 hash_misses=16 rule_drop=16 forward=16 mark=16 default_drop=16 invalid=0
FLOW_LATENCY samples=64 p50_cycles=75 p99_cycles=275 max_cycles=975 p99_ns=110
APP_RESULT rx=64 tx=32 tx_failed=0 freed=32
FLOW_RULE_ADD_UPDATE_DELETE_PASS
FLOW_RULE_AGING_PASS
DPDK_FLOW_PIPELINE_PHASE2_PASS
DPDK_FLOW_PIPELINE_PHASE1_PASS
DPDK_FLOW_PIPELINE_PHASE2_LIFECYCLE_PASS
cleanup=complete result=pass
script_summary name=flow_pipeline_smoke status=pass
```

## 4. 结论与边界

规则生命周期 PASS。update 保持 rule 地址稳定并递增 generation；delete/aging 删除 hash key并释放逻辑槽位，后续 add 可复用。aging 使用合成 TSC 做确定性自测。

当前仍是单进程、单 RX queue；多 worker 并发 update/lookup 需要 Phase 3 明确 shared table、分片 table 或 RCU/QSBR 同步模型。
