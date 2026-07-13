# CQ Polling Budget Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a client-side CQ polling budget dimension that can compare `ibv_poll_cq()` one-by-one versus burst polling without changing the project's existing default behavior.

**Architecture:** Keep the current measurement boundary unchanged and introduce one new variable, `PERF_POLL_CQ_BUDGET`. The client batch polling loop will use `min(remaining, poll_budget)` per `ibv_poll_cq()` call, and the existing sweep/CSV/summary pipeline will be extended with a dedicated poll-budget matrix.

**Tech Stack:** C11, libibverbs, bash, GNU make, existing RDMA RC helper library.

---

### Task 1: Add poll-budget parsing and logging fields

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/src/perf_common.h`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/src/perf_client.c`

- [ ] **Step 1: Add shared poll-budget helpers**

Add to `src/perf_common.h`:

```c
#define PERF_DEFAULT_POLL_CQ_BUDGET PERF_MAX_BATCH_SIZE

static inline int perf_get_poll_cq_budget(void)
{
    return perf_parse_positive_env("PERF_POLL_CQ_BUDGET",
                                   PERF_DEFAULT_POLL_CQ_BUDGET,
                                   PERF_MAX_BATCH_SIZE);
}

static inline const char *perf_poll_mode(int poll_budget)
{
    return poll_budget == 1 ? "single" : "burst";
}

static inline int perf_poll_batch(int remaining, int poll_budget)
{
    return remaining < poll_budget ? remaining : poll_budget;
}
```

- [ ] **Step 2: Thread poll budget through client config**

In `src/perf_client.c`, load:

```c
int poll_budget = perf_get_poll_cq_budget();
```

and append to the existing config lines:

```c
printf("perf_config ... poll_mode=%s poll_budget=%d\n",
       perf_poll_mode(poll_budget), poll_budget);
```

- [ ] **Step 3: Update the batch polling loop**

Change the client batch CQ polling call from:

```c
int polled = ibv_poll_cq(context->cq, expected_cqes - completed, wc);
```

to:

```c
int poll_batch = perf_poll_batch(expected_cqes - completed, poll_budget);
int polled = ibv_poll_cq(context->cq, poll_batch, wc);
```

and update the helper signature accordingly.

- [ ] **Step 4: Extend result markers**

Append `poll_mode` and `poll_budget` to:

```c
perf_result
perf_throughput
perf_compare
```

so they expose:

```text
poll_mode=single|burst
poll_budget=<N>
```

### Task 2: Extend smoke test and CSV export

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/perf_smoke_test.sh`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/export_perf_csv.sh`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/grep_perf_markers.sh`

- [ ] **Step 1: Pass poll budget into smoke test**

In `tests/perf_smoke_test.sh`, add:

```bash
PERF_POLL_CQ_BUDGET="${PERF_POLL_CQ_BUDGET:-16}"
```

include it in `script_config`, and pass it to both server/client process launches:

```bash
PERF_POLL_CQ_BUDGET="${PERF_POLL_CQ_BUDGET}" \
./build/rdma-perf-client ...
```

- [ ] **Step 2: Add marker assertions**

Append checks:

```bash
grep -q "poll_budget=${PERF_POLL_CQ_BUDGET}" tests/perf-client.log
grep -q 'poll_mode=' tests/perf-client.log
```

- [ ] **Step 3: Extend summary CSV**

In `tests/export_perf_csv.sh`, extend the header with:

```text
poll_mode,poll_budget
```

and append extracted values from `perf_compare`:

```bash
"$(extract_field "${compare_line}" poll_mode)" \
"$(extract_field "${compare_line}" poll_budget)"
```

- [ ] **Step 4: Extend marker grep**

In `tests/grep_perf_markers.sh`, include:

```text
poll_mode|poll_budget
```

### Task 3: Extend sweep path helpers and base sweep scripts

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/perf_mode_helpers.sh`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/run_perf_sweep.sh`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/summarize_sweep.sh`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/check_sweep_csv.sh`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/check_sweep_artifacts.sh`

- [ ] **Step 1: Add optional poll suffix**

In `tests/perf_mode_helpers.sh`, change helpers to accept a third argument:

```bash
local poll_budget="${3:-16}"
```

and append:

```bash
if [[ "${poll_budget}" != "16" ]]; then
  suffix="${suffix}-poll${poll_budget}"
fi
```

- [ ] **Step 2: Thread poll budget through base sweep**

In `tests/run_perf_sweep.sh`, add:

```bash
SWEEP_POLL_CQ_BUDGET="${SWEEP_POLL_CQ_BUDGET:-16}"
```

use it in:

```bash
OUT_CSV="$(perf_sweep_csv_path "${SWEEP_USE_INLINE}" "${SWEEP_SIGNAL_INTERVAL}" "${SWEEP_POLL_CQ_BUDGET}")"
SWEEP_DIR="$(perf_sweep_dir_path "${SWEEP_USE_INLINE}" "${SWEEP_SIGNAL_INTERVAL}" "${SWEEP_POLL_CQ_BUDGET}")"
```

and pass to smoke test:

```bash
PERF_POLL_CQ_BUDGET="${SWEEP_POLL_CQ_BUDGET}" \
bash tests/perf_smoke_test.sh
```

- [ ] **Step 3: Reflect poll budget in summary/check scripts**

Add:

```bash
SWEEP_POLL_CQ_BUDGET="${SWEEP_POLL_CQ_BUDGET:-16}"
```

to `summarize_sweep.sh`, `check_sweep_csv.sh`, and `check_sweep_artifacts.sh`, and resolve paths using the third helper argument.

- [ ] **Step 4: Mention poll budget in markdown summary**

In `tests/summarize_sweep.sh`, add:

```text
- poll mode: single|burst
- poll budget: <N>
```

derived from the sweep env.

### Task 4: Add dedicated poll-budget matrix scripts

**Files:**
- Create: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/run_poll_budget_sweep.sh`
- Create: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/summarize_poll_budget_sweep.sh`
- Create: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/check_poll_budget_summary.sh`

- [ ] **Step 1: Add matrix driver**

Create `tests/run_poll_budget_sweep.sh` modeled after the signal-interval sweep:

```bash
POLL_CQ_BUDGETS="${POLL_CQ_BUDGETS:-1 2 4 8 16}"
```

and for each budget run:

```bash
SWEEP_POLL_CQ_BUDGET="${poll_budget}" bash tests/run_perf_sweep.sh
```

then append a row:

```text
poll_budget,inline_mode,best_tp_batch_size,best_msg_per_sec,best_lat_batch_size,best_batch_avg_msg_ns,best_speed_batch_size,best_speedup_x100,source_csv
```

- [ ] **Step 2: Add matrix summary**

Create `tests/summarize_poll_budget_sweep.sh` that emits:

```text
tests/perf-poll-budget-summary.md
tests/perf-poll-budget-inline-summary.md
```

with best throughput / latency / speedup budgets and a per-budget table.

- [ ] **Step 3: Add matrix structure checker**

Create `tests/check_poll_budget_summary.sh` that verifies:

```text
matrix csv exists
summary md exists
line_count >= 2
expected headings exist
```

### Task 5: Wire Makefile and docs/tests records

**Files:**
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/Makefile`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/README.md`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/docs/ARCHITECTURE.md`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/docs/DEEP_LEARNING.md`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/docs/TEST_FLOW.md`
- Modify: `linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/TEST_RECORD_20260711.md`

- [ ] **Step 1: Add Makefile targets**

Add variables:

```make
POLL_CQ_BUDGETS ?= 1 2 4 8 16
```

and targets:

```make
pollsweep
inlinepollsweep
pollsummary
inlinepollsummary
pollreport
inlinepollreport
```

plus clean rules for:

```text
tests/perf-poll-budget*.csv
tests/perf-poll-budget*.md
```

- [ ] **Step 2: Update docs**

Document:

- new variable `PERF_POLL_CQ_BUDGET`
- `single` vs `burst` polling meaning
- new commands `make pollreport` / `make inlinepollreport`
- new artifact files

- [ ] **Step 3: Update test record**

Append the exact remote commands and final observed results once verification completes.

### Task 6: Verify on host 135

**Files:**
- Test only

- [ ] **Step 1: Quick isolated checks**

Run:

```bash
SUDO_PASSWORD='wq123456!' PERF_POLL_CQ_BUDGET=1 make quickreport
SUDO_PASSWORD='wq123456!' PERF_POLL_CQ_BUDGET=4 make quickreport
```

Expected:

```text
PASS: RDMA SEND latency + batch WR smoke test ...
poll_budget=1
poll_budget=4
```

- [ ] **Step 2: Normal poll matrix**

Run:

```bash
SUDO_PASSWORD='wq123456!' make pollreport POLL_CQ_BUDGETS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

Expected:

```text
poll_budget_sweep=pass
poll_budget_summary=pass
poll_budget_check=pass
```

- [ ] **Step 3: Inline poll matrix**

Run:

```bash
SUDO_PASSWORD='wq123456!' make inlinepollreport POLL_CQ_BUDGETS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

Expected:

```text
poll_budget_sweep=pass
poll_budget_summary=pass
poll_budget_check=pass
```

- [ ] **Step 4: Record the winning budgets**

Copy the best normal and inline numbers into:

```text
README.md
docs/ARCHITECTURE.md
tests/TEST_RECORD_20260711.md
```
