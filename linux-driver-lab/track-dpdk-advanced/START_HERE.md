# START_HERE

`track-dpdk-advanced` 已完成阶段性收敛。

## 先读这几个文件

```text
README.md
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

