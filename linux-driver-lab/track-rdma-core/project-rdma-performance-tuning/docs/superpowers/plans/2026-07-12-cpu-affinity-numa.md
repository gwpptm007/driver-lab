# CPU Affinity / NUMA Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add optional CPU affinity / NUMA launch controls and runtime evidence logging to the RDMA performance-tuning project without changing the RDMA datapath.

**Architecture:** Keep binding at the shell launcher layer via `taskset` / `numactl`, then let the server/client log their observed runtime CPU and allowed CPU/memory masks. Reuse the same env variables across local smoke, dual-host scripts, and docs so the feature stays composable with existing batch/inline/selective/poll/RTT modes.

**Tech Stack:** C11, bash, libibverbs, Linux `/proc/self/status`, `taskset`, `numactl`

---

### Task 1: Add shared affinity / NUMA helpers

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/src/perf_common.h`

- [ ] Add env-name helper functions for `PERF_SERVER_CPUSET`, `PERF_CLIENT_CPUSET`, `PERF_SERVER_NUMA_NODE`, `PERF_CLIENT_NUMA_NODE`.
- [ ] Add runtime-report helpers that print `perf_binding role=<role> current_cpu=<n> cpus_allowed=<list> mems_allowed=<list>`.
- [ ] Keep code Linux-friendly and no-op gracefully if a field cannot be read.

### Task 2: Add launcher helper script

**Files:**
- Create: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/perf_launch_helpers.sh`

- [ ] Add bash helpers to compose launcher arrays for `taskset -c` and optional `numactl --cpunodebind= --membind=`.
- [ ] Add one helper to print a `script_binding` summary line for server/client.
- [ ] Keep helper API reusable by local smoke and dual-host scripts.

### Task 3: Wire local smoke and dual-host scripts

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/perf_smoke_test.sh`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/dual_perf_server.sh`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/dual_perf_client.sh`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/check_env.sh`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/Makefile`

- [ ] Source the new launcher helper in each script.
- [ ] Add new env vars to `script_config`.
- [ ] Start server/client through launcher arrays instead of direct binary exec.
- [ ] Extend `envcheck` with topology output and affinity-related env echo.
- [ ] Add help text for new env vars.

### Task 4: Emit runtime binding evidence from server/client

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/src/perf_server.c`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/src/perf_client.c`

- [ ] Print one `perf_binding` line near startup after config logging.
- [ ] Include server/client requested env values in existing config print if useful.
- [ ] Do not change the timing window or CQ logic.

### Task 5: Update docs and records

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/README.md`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/docs/RTT_DUAL_HOST.md`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/docs/TEST_FLOW.md`
- Create or Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/TEST_RECORD_20260712.md`
- Modify: `linux-driver-lab/track-rdma-core/ROADMAP.md`

- [ ] Document the new env vars and the “record, not conclusion” boundary.
- [ ] Add at least one Mermaid/UML section explaining launcher-layer binding.
- [ ] Record the exact 135 validation commands and outputs.

### Task 6: Fresh verification on 135

**Files:**
- Verify only

- [ ] Run `make` on `192.168.65.135`.
- [ ] Run `bash tests/check_env.sh`.
- [ ] Run one small sample with `PERF_SERVER_CPUSET` / `PERF_CLIENT_CPUSET`.
- [ ] Verify original PASS markers plus new `perf_binding` / `script_binding` evidence.
- [ ] If NUMA is single-node, record that honestly instead of forcing a comparison.
