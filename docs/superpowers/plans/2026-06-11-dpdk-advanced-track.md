# DPDK Advanced Track Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create the `track-dpdk-advanced` planning layer and prepare Phase 1 for mbuf/mempool implementation.

**Architecture:** Keep RDMA and SmartNIC/DPU as roadmap-level follow-ups while making DPDK Advanced the next executable track. The first executable lab is `lab-dpdk-mbuf-mempool-deep-dive`; later labs cover RSS/multiqueue, NUMA/burst tuning, VFIO/IOMMU boundaries, L3 forwarding, and final summary.

**Tech Stack:** DPDK 21.11 concepts, pcap PMD, vmxnet3 evidence, C/meson for future apps, shell scripts for future execution, Markdown records and reports.

---

### Task 1: Acceleration Roadmap

**Files:**
- Create: `linux-driver-lab/docs/08_ACCELERATION_ROADMAP.md`
- Modify: `README.md`
- Modify: `linux-driver-lab/README.md`
- Modify: `linux-driver-lab/track-block-io/README.md`
- Modify: `linux-driver-lab/track-block-io/START_HERE.md`

- [ ] **Step 1: Confirm acceleration roadmap exists**

Run: `Test-Path linux-driver-lab/docs/08_ACCELERATION_ROADMAP.md`

Expected: `True`

- [ ] **Step 2: Confirm block I/O is parked**

Run: `rg -n "PARKED_PLANNED|P2 保留支线" linux-driver-lab/track-block-io`

Expected: `PARKED_PLANNED` appears in `README.md` and `START_HERE.md`.

- [ ] **Step 3: Confirm root navigation mentions DPDK Advanced**

Run: `rg -n "track-dpdk-advanced|DPDK Advanced|SmartNIC|RDMA" README.md linux-driver-lab/README.md linux-driver-lab/docs/08_ACCELERATION_ROADMAP.md`

Expected: all three files mention the acceleration direction.

### Task 2: DPDK Advanced Skeleton

**Files:**
- Create: `linux-driver-lab/track-dpdk-advanced/README.md`
- Create: `linux-driver-lab/track-dpdk-advanced/START_HERE.md`
- Create: `linux-driver-lab/track-dpdk-advanced/ROADMAP.md`
- Create: `linux-driver-lab/track-dpdk-advanced/docs/00_TRACK_OVERVIEW.md`
- Create: `linux-driver-lab/track-dpdk-advanced/docs/01_DPDK_ADVANCED_MODEL.md`
- Create: `linux-driver-lab/track-dpdk-advanced/docs/02_LAB_PLAN.md`
- Create: `linux-driver-lab/track-dpdk-advanced/docs/03_ACCEPTANCE.md`
- Create: `linux-driver-lab/track-dpdk-advanced/docs/04_RDMA_SMARTNIC_BRIDGE.md`

- [ ] **Step 1: Confirm skeleton files exist**

Run: `Get-ChildItem -Recurse linux-driver-lab/track-dpdk-advanced | Select-Object FullName`

Expected: README, START_HERE, ROADMAP, and docs files are listed.

- [ ] **Step 2: Confirm track is planned, not completed**

Run: `rg -n "PLANNED|不要夸大|不说完成生产级|不说当前环境" linux-driver-lab/track-dpdk-advanced`

Expected: boundary wording is present.

### Task 3: Phase 1 Lab Planning

**Files:**
- Create: `linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive/README.md`
- Create: `linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive/docs/01_LAB_OVERVIEW.md`

- [ ] **Step 1: Confirm Phase 1 lab overview exists**

Run: `Test-Path linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive/docs/01_LAB_OVERVIEW.md`

Expected: `True`

- [ ] **Step 2: Confirm Phase 1 acceptance labels**

Run: `rg -n "PASS_BUILD|PASS_PCAP_RX|PASS_MBUF_METADATA|PASS_MEMPOOL_CONFIG|PASS_STATS_CONSISTENCY" linux-driver-lab/track-dpdk-advanced`

Expected: all five labels appear.

### Task 4: Future Implementation Handoff

**Files:**
- Future create: `linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive/app/main.c`
- Future create: `linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive/app/meson.build`
- Future create: `linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive/scripts/00_check_env.sh`
- Future create: `linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive/scripts/01_build.sh`
- Future create: `linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive/scripts/02_run_pcap_metadata.sh`
- Future create: `linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive/scripts/03_collect_report.sh`

- [ ] **Step 1: Start implementation from Phase 1 overview**

Run: `Get-Content -Encoding UTF8 linux-driver-lab/track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive/docs/01_LAB_OVERVIEW.md`

Expected: overview lists the fields to observe and boundaries to avoid.

- [ ] **Step 2: Build first failing smoke**

Before writing app logic, create `02_run_pcap_metadata.sh` so it fails clearly when the app binary is missing.

Expected failing output before implementation:

```text
MISS app/build/dpdk-mbuf-inspect
```

- [ ] **Step 3: Implement only metadata inspection first**

The first implementation should print mbuf metadata for pcap PMD traffic. It should not attempt RSS, NUMA, VFIO, or L3 forwarding.

