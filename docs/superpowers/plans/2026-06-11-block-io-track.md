# Block I/O Track Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a new `track-block-io` learning track, starting with a minimal ramdisk block driver and ending with a block I/O portfolio summary.

**Architecture:** The track follows the existing lab/project pattern in `linux-driver-lab`: each lab owns its README, docs, scripts, records, and reports, while `project-block-io-track-summary` gathers final portfolio material. Phase 1 starts with a teaching ramdisk driver before moving to request-path explanation, blk-mq, real-driver source dive, and observability.

**Tech Stack:** Linux kernel module C, block layer APIs, blk-mq, shell scripts, fio, iostat, blktrace, bpftrace, Markdown records/reports.

---

### Task 1: Track Skeleton

**Files:**
- Create: `linux-driver-lab/track-block-io/README.md`
- Create: `linux-driver-lab/track-block-io/START_HERE.md`
- Create: `linux-driver-lab/track-block-io/ROADMAP.md`
- Create: `linux-driver-lab/track-block-io/docs/00_TRACK_OVERVIEW.md`
- Create: `linux-driver-lab/track-block-io/docs/01_BLOCK_LAYER_MODEL.md`
- Create: `linux-driver-lab/track-block-io/docs/02_IMPLEMENTATION_PLAN.md`
- Create: `linux-driver-lab/track-block-io/docs/03_ACCEPTANCE.md`

- [ ] **Step 1: Confirm skeleton files exist**

Run: `Test-Path linux-driver-lab/track-block-io/README.md`

Expected: `True`

- [ ] **Step 2: Review track status wording**

Run: `rg -n "PLANNED|PASS|生产级|NVMe" linux-driver-lab/track-block-io`

Expected: files describe the track as planned and avoid production-grade claims.

- [ ] **Step 3: Commit skeleton**

```bash
git add linux-driver-lab/track-block-io docs/superpowers/plans/2026-06-11-block-io-track.md
git commit -m "docs: add block io track plan"
```

### Task 2: Phase 1 Lab Planning

**Files:**
- Create: `linux-driver-lab/track-block-io/lab-simple-ramdisk/README.md`
- Create: `linux-driver-lab/track-block-io/lab-simple-ramdisk/docs/01_LAB_OVERVIEW.md`

- [ ] **Step 1: Confirm lab overview exists**

Run: `Test-Path linux-driver-lab/track-block-io/lab-simple-ramdisk/docs/01_LAB_OVERVIEW.md`

Expected: `True`

- [ ] **Step 2: Confirm Phase 1 acceptance appears in docs**

Run: `rg -n "PASS_REGISTER|PASS_READ_WRITE|PASS_MKFS|PASS_MOUNT|PASS_FIO_SMOKE|PASS_CLEANUP" linux-driver-lab/track-block-io`

Expected: all Phase 1 acceptance labels are present.

- [ ] **Step 3: Commit lab planning**

```bash
git add linux-driver-lab/track-block-io/lab-simple-ramdisk
git commit -m "docs: plan simple ramdisk block lab"
```

### Task 3: Repository Navigation

**Files:**
- Modify: `README.md`
- Modify: `linux-driver-lab/README.md`

- [ ] **Step 1: Confirm root navigation mentions block I/O**

Run: `rg -n "block I/O|track-block-io|storage I/O" README.md linux-driver-lab/README.md`

Expected: both root README files mention the new planned track.

- [ ] **Step 2: Commit navigation updates**

```bash
git add README.md linux-driver-lab/README.md
git commit -m "docs: link block io track from project readmes"
```

### Task 4: First Implementation Handoff

**Files:**
- Future create: `linux-driver-lab/track-block-io/lab-simple-ramdisk/driver/labram.c`
- Future create: `linux-driver-lab/track-block-io/lab-simple-ramdisk/driver/Makefile`
- Future create: `linux-driver-lab/track-block-io/lab-simple-ramdisk/scripts/00_check_env.sh`
- Future create: `linux-driver-lab/track-block-io/lab-simple-ramdisk/scripts/01_build.sh`
- Future create: `linux-driver-lab/track-block-io/lab-simple-ramdisk/scripts/02_run_smoke.sh`
- Future create: `linux-driver-lab/track-block-io/lab-simple-ramdisk/scripts/03_clean.sh`

- [ ] **Step 1: Start implementation from the lab overview**

Run: `Get-Content -Encoding UTF8 linux-driver-lab/track-block-io/lab-simple-ramdisk/docs/01_LAB_OVERVIEW.md`

Expected: overview contains commands and boundary wording needed to implement the lab.

- [ ] **Step 2: Use TDD-style smoke first**

Before writing driver code, create `02_run_smoke.sh` that fails with a clear message if `/dev/labram0` is missing.

Expected failing output before implementation:

```text
MISS /dev/labram0
```

- [ ] **Step 3: Implement the smallest driver that passes registration**

Implement only device registration first. Do not add mkfs/fio support until `PASS_REGISTER` is recorded.

