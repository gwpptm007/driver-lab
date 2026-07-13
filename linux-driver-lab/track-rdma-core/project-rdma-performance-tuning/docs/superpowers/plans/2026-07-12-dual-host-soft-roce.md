# Dual-Host Soft-RoCE Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add dual-host Soft-RoCE execution to the perf project and introduce a first RTT phase while preserving existing completion-latency behavior.

**Architecture:** Reuse the proven `134 -> 135 / ens33 / gid_index=1` path from `project-rdma-rc-client-server`. Keep completion latency untouched, then append an optional RTT phase gated by `PERF_ENABLE_RTT`, and expose it through dual-host test scripts.

**Tech Stack:** C11, libibverbs, bash, GNU make, existing RDMA RC helper library.

---

### Task 1: Add RTT environment parsing and payload helpers

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/src/perf_common.h`

- [ ] Add `PERF_ENABLE_RTT`, request/response payload constants, and helper name functions.

### Task 2: Implement optional RTT phase in `perf_server.c`

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/src/perf_server.c`

- [ ] After the existing batch phase, accept either `PERF_DONE` or `START_RTT`.
- [ ] For RTT mode, receive request, validate payload, send response, and poll local SEND CQE.

### Task 3: Implement optional RTT phase in `perf_client.c`

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/src/perf_client.c`

- [ ] Keep current completion and batch behavior unchanged by default.
- [ ] When `PERF_ENABLE_RTT=1`, run a request/response RTT loop and print RTT stats/markers.

### Task 4: Extend smoke test and add dual-host scripts

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/perf_smoke_test.sh`
- Create: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/dual_perf_server.sh`
- Create: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/dual_perf_client.sh`

- [ ] Support optional RTT assertions in local smoke test.
- [ ] Add dual-host perf server/client wrappers using `ens33` and `gid_index=1` defaults.

### Task 5: Wire Makefile and docs

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/Makefile`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/README.md`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/docs/ARCHITECTURE.md`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/docs/DEEP_LEARNING.md`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/docs/TEST_FLOW.md`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/TEST_RECORD_20260711.md`

- [ ] Add `dual-server` / `dual-client` targets.
- [ ] Document the dual-host path and the RTT phase.

### Task 6: Verify on reachable hosts

**Files:**
- Test only

- [ ] Verify single-host build and optional RTT smoke on `135`.
- [ ] If `134` is reachable, run dual-server on `135` and dual-client on `134`.
- [ ] If `134` is not reachable from this session, record that the scripts are prepared and note the exact commands for manual execution.
