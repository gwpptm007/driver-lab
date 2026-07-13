# TEST_RECORD_20260713_PHASE3_WORKER

## 1. 目标

验证 main lcore、两个 SP/SC `rte_ring`、两个 worker lcore、per-worker stats，以及 shared-readonly 和 sharded flow table 两种模型。

## 2. 环境与命令

- 主机：`192.168.65.135`
- DPDK：`21.11.9`
- lcores：`0-2`，main 0，worker 1/2
- NUMA：单 node

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/project-dpdk-flow-pipeline
make clean
make
make test

grep -E 'FLOW_WORKER_RESULT|FLOW_WORKER_.*PASS|PHASE3' \
  tests/runtime/flow_pipeline.log
```

## 3. 关键证据

```text
FLOW_WORKER_RESULT mode=shared queue0=32 queue1=32 hits=48 misses=16 drop=16 forward=16 mark=16 default_drop=16
FLOW_WORKER_SHARED_TABLE_PASS
FLOW_WORKER_RESULT mode=sharded queue0=32 queue1=32 hits=48 misses=16 drop=16 forward=16 mark=16 default_drop=16
FLOW_WORKER_SHARDED_TABLE_PASS
DPDK_FLOW_PIPELINE_PHASE3_PASS
DPDK_FLOW_PIPELINE_PHASE3_WORKER_PASS
cleanup=complete result=pass
PASS: DPDK rte_hash flow actions and latency smoke
script_summary name=flow_pipeline_smoke status=pass
```

最终编译日志未出现 `warning:`。

## 4. 结论与边界

双 worker 的 shared/sharded table 模型 PASS，每个 logical queue 处理 32 包，聚合动作和 Phase 1 完全一致。shared table 运行期只读，rule counter 使用 relaxed atomic；sharded table 每 worker 独立拥有。

由于 pcap PMD 只有一个 RX queue，queue 选择由 main lcore 根据源地址最低位完成。该结果证明软件 queue-to-lcore 模型，不证明 NIC RSS、RETA 或硬件多 RX queue。
