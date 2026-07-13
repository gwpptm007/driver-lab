# START_HERE

`track-dpdk-advanced` 已完成阶段性收敛。

## 先做基础门槛检查

如果不能解释 descriptor/mbuf/buffer、TX partial ownership、ethdev capability 和 pcap/真实 NIC 证据边界，先读：

```text
../track-dpdk/docs/fundamentals/00_10_MINUTE_MENTAL_MODEL.md
../track-dpdk/docs/fundamentals/08_PACKET_FORMAT_AND_OFFLOADS.md
../track-dpdk/docs/fundamentals/09_ETHDEV_CAPABILITY_AND_PORT_CONFIG.md
../track-dpdk/docs/fundamentals/10_PERFORMANCE_AND_OBSERVABILITY.md
```

基础清楚后，从 `docs/fundamentals/00_ADVANCED_MENTAL_MODEL.md` 开始进阶路线。

## 先读这几个文件

```text
README.md
docs/fundamentals/00_ADVANCED_MENTAL_MODEL.md
docs/fundamentals/01_HARDWARE_QUEUE_STEERING.md
docs/fundamentals/02_ADVANCED_MEMORY_AND_DATA_STRUCTURES.md
docs/fundamentals/03_MULTICORE_PIPELINE_DATA_PATH.md
docs/fundamentals/04_CONCURRENCY_RCU_QSBR.md
docs/01_TRACK_OVERVIEW.md
docs/04_ARCHITECTURE_PRINCIPLES.md
project-dpdk-advanced-summary/reports/DPDK_ADVANCED_FINAL_REPORT.md
project-dpdk-advanced-summary/reports/EVIDENCE_INDEX.md
```

## 如果要看代码

```text
lab-dpdk-mbuf-mempool-deep-dive/app/main.c
lab-dpdk-rss-multiqueue/app/main.c
lab-dpdk-numa-burst-tuning/app/main.c
project-dpdk-l3-forwarder-lite/app/main.c
project-dpdk-flow-pipeline/app/main.c
project-dpdk-flow-pipeline/app/flow_table.c
project-dpdk-flow-pipeline/app/flow_pipeline.c
```

## 如果要复跑测试

每个 lab/project 都保持同一套结构：

```text
scripts/00_check_env.sh
scripts/01_build.sh
scripts/02_*.sh
scripts/03_collect_report.sh
docs/01_OVERVIEW.md
docs/02_TEST_AND_VERIFY.md
docs/03_RESULT_ANALYSIS.md
docs/04_DEEP_LEARNING.md
records/<timestamp>/
reports/*.md
```

优先复跑 Phase 5：

```bash
cd linux-driver-lab/track-dpdk-advanced/project-dpdk-l3-forwarder-lite
chmod +x scripts/*.sh tools/*.py
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_l3_forward.sh
./scripts/03_collect_report.sh
```

当前环境完整软件 pipeline 回归：

```bash
cd linux-driver-lab/track-dpdk-advanced/project-dpdk-flow-pipeline
make test-all
```

track 级文档与回归入口见 `tests/TEST_FLOW.md`。
