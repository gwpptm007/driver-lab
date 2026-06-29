# START_HERE

DPDK Advanced 宸插畬鎴愰樁娈垫€ф敹鏁涖€?
## 鍏堣杩欎笁涓枃浠?
```text
README.md
docs/04_ARCHITECTURE_PRINCIPLES.md
project-dpdk-advanced-summary/reports/DPDK_ADVANCED_FINAL_REPORT.md
project-dpdk-advanced-summary/reports/EVIDENCE_INDEX.md
```

## 濡傛灉瑕佺湅浠ｇ爜

```text
lab-dpdk-mbuf-mempool-deep-dive/app/main.c
lab-dpdk-rss-multiqueue/app/main.c
lab-dpdk-numa-burst-tuning/app/main.c
project-dpdk-l3-forwarder-lite/app/main.c
```

## 濡傛灉瑕佸璺戞祴璇?
姣忎釜 lab/project 閮芥湁鐩稿悓缁撴瀯锛?
```text
scripts/00_check_env.sh
scripts/01_build.sh
scripts/02_*.sh
scripts/03_collect_report.sh
docs/04_DEEP_LEARNING.md
docs/02_TEST_AND_VERIFY.md
records/<timestamp>/
reports/*.md
```

浼樺厛澶嶈窇 Phase 5锛?
```bash
cd linux-driver-lab/track-dpdk-advanced/project-dpdk-l3-forwarder-lite
chmod +x scripts/*.sh tools/*.py
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_l3_forward.sh
./scripts/03_collect_report.sh
```
