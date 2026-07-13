BASE := ../project-rdma-rc-client-server
SERVER := build/rdma-perf-server
CLIENT := build/rdma-perf-client
COMMON_SRC := $(BASE)/src/common.c $(BASE)/src/control_plane.c $(BASE)/src/rdma_context.c
PERF_COMMON_HDR := src/perf_common.h
SERVER_SRC := src/perf_server.c $(COMMON_SRC)
CLIENT_SRC := src/perf_client.c $(COMMON_SRC)
PC := $(shell pkg-config --cflags libibverbs 2>/dev/null)
PL := $(shell pkg-config --libs libibverbs 2>/dev/null)
QUICK_ITERATIONS ?= 16
QUICK_BATCH_SIZE ?= 4
FULL_ITERATIONS ?= 1000
FULL_BATCH_SIZE ?= 8
SELECTIVE_SIGNAL_INTERVAL ?= 4
SIGNAL_INTERVALS ?= 1 2 4 8 16
POLL_CQ_BUDGETS ?= 1 2 4 8 16
SWEEP_ITERATIONS ?= $(FULL_ITERATIONS)
SWEEP_BATCH_SIZES ?= 1 2 4 8 16

CPPFLAGS += -I$(BASE)/include $(PC)
CFLAGS += -O2 -g -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += $(if $(strip $(PL)),$(PL),-libverbs)

.PHONY: all test quicktest fulltest quickcheck fullcheck quickdiag fulldiag inlinequicktest inlinefulltest inlinequickcheck inlinefullcheck inlinequickreport inlinefullreport selectivequicktest selectivefulltest selectivequickcheck selectivefullcheck selectivequickreport selectivefullreport selectiveinlinequicktest selectiveinlinefulltest selectiveinlinequickcheck selectiveinlinefullcheck selectiveinlinequickreport selectiveinlinefullreport markers report quickreport fullreport sweep inlinesweep selectivesweep selectiveinlinesweep signalsweep inlinesignalsweep pollsweep inlinepollsweep sweepsummary inlinesweepsummary selectivesweepsummary selectiveinlinesweepsummary signalsummary inlinesignalsummary pollsummary inlinepollsummary sweepreport inlinesweepreport selectivesweepreport selectiveinlinesweepreport signalreport inlinesignalreport pollreport inlinepollreport comparesweeps comparecheck compareselective compareselectivecheck compareinlineselective compareinlineselectivecheck batchphaseclose selectivephaseclose selectiveinlinephaseclose pollphaseclose finalphaseclose dual-server dual-client envcheck help clean

all: $(SERVER) $(CLIENT)

$(SERVER): $(SERVER_SRC) $(BASE)/include/rdma_cs.h $(PERF_COMMON_HDR) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SERVER_SRC) $(LDLIBS)

$(CLIENT): $(CLIENT_SRC) $(BASE)/include/rdma_cs.h $(PERF_COMMON_HDR) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(CLIENT_SRC) $(LDLIBS)

build:
	mkdir -p $@

test: all
	bash tests/perf_smoke_test.sh

quicktest: all
	PERF_ITERATIONS=$(QUICK_ITERATIONS) PERF_BATCH_SIZE=$(QUICK_BATCH_SIZE) bash tests/perf_smoke_test.sh

fulltest: all
	PERF_ITERATIONS=$(FULL_ITERATIONS) PERF_BATCH_SIZE=$(FULL_BATCH_SIZE) bash tests/perf_smoke_test.sh

quickcheck: quicktest
	bash tests/grep_perf_markers.sh

fullcheck: fulltest
	bash tests/grep_perf_markers.sh

quickdiag: envcheck quickcheck

fulldiag: envcheck fullcheck

inlinequicktest: all
	PERF_ITERATIONS=$(QUICK_ITERATIONS) PERF_BATCH_SIZE=$(QUICK_BATCH_SIZE) PERF_USE_INLINE=1 bash tests/perf_smoke_test.sh

inlinefulltest: all
	PERF_ITERATIONS=$(FULL_ITERATIONS) PERF_BATCH_SIZE=$(FULL_BATCH_SIZE) PERF_USE_INLINE=1 bash tests/perf_smoke_test.sh

inlinequickcheck: inlinequicktest
	bash tests/grep_perf_markers.sh

inlinefullcheck: inlinefulltest
	bash tests/grep_perf_markers.sh

inlinequickreport: envcheck inlinequickcheck report

inlinefullreport: envcheck inlinefullcheck report

selectivequicktest: all
	PERF_ITERATIONS=$(QUICK_ITERATIONS) PERF_BATCH_SIZE=$(QUICK_BATCH_SIZE) PERF_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL) bash tests/perf_smoke_test.sh

selectivefulltest: all
	PERF_ITERATIONS=$(FULL_ITERATIONS) PERF_BATCH_SIZE=$(FULL_BATCH_SIZE) PERF_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL) bash tests/perf_smoke_test.sh

selectivequickcheck: selectivequicktest
	bash tests/grep_perf_markers.sh

selectivefullcheck: selectivefulltest
	bash tests/grep_perf_markers.sh

selectivequickreport: envcheck selectivequickcheck report

selectivefullreport: envcheck selectivefullcheck report

selectiveinlinequicktest: all
	PERF_ITERATIONS=$(QUICK_ITERATIONS) PERF_BATCH_SIZE=$(QUICK_BATCH_SIZE) PERF_USE_INLINE=1 PERF_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL) bash tests/perf_smoke_test.sh

selectiveinlinefulltest: all
	PERF_ITERATIONS=$(FULL_ITERATIONS) PERF_BATCH_SIZE=$(FULL_BATCH_SIZE) PERF_USE_INLINE=1 PERF_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL) bash tests/perf_smoke_test.sh

selectiveinlinequickcheck: selectiveinlinequicktest
	bash tests/grep_perf_markers.sh

selectiveinlinefullcheck: selectiveinlinefulltest
	bash tests/grep_perf_markers.sh

selectiveinlinequickreport: envcheck selectiveinlinequickcheck report

selectiveinlinefullreport: envcheck selectiveinlinefullcheck report

markers:
	bash tests/grep_perf_markers.sh

report:
	bash tests/export_perf_csv.sh

quickreport: quickdiag report

fullreport: fulldiag report

sweep: all
	SWEEP_ITERATIONS=$(SWEEP_ITERATIONS) SWEEP_BATCH_SIZES="$(SWEEP_BATCH_SIZES)" bash tests/run_perf_sweep.sh

inlinesweep: all
	SWEEP_ITERATIONS=$(SWEEP_ITERATIONS) SWEEP_BATCH_SIZES="$(SWEEP_BATCH_SIZES)" SWEEP_USE_INLINE=1 bash tests/run_perf_sweep.sh

selectivesweep: all
	SWEEP_ITERATIONS=$(SWEEP_ITERATIONS) SWEEP_BATCH_SIZES="$(SWEEP_BATCH_SIZES)" SWEEP_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL) bash tests/run_perf_sweep.sh

selectiveinlinesweep: all
	SWEEP_ITERATIONS=$(SWEEP_ITERATIONS) SWEEP_BATCH_SIZES="$(SWEEP_BATCH_SIZES)" SWEEP_USE_INLINE=1 SWEEP_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL) bash tests/run_perf_sweep.sh

signalsweep: all
	SWEEP_ITERATIONS=$(SWEEP_ITERATIONS) SWEEP_BATCH_SIZES="$(SWEEP_BATCH_SIZES)" SIGNAL_INTERVALS="$(SIGNAL_INTERVALS)" bash tests/run_signal_interval_sweep.sh

inlinesignalsweep: all
	SWEEP_ITERATIONS=$(SWEEP_ITERATIONS) SWEEP_BATCH_SIZES="$(SWEEP_BATCH_SIZES)" SWEEP_USE_INLINE=1 SIGNAL_INTERVALS="$(SIGNAL_INTERVALS)" bash tests/run_signal_interval_sweep.sh

pollsweep: all
	SWEEP_ITERATIONS=$(SWEEP_ITERATIONS) SWEEP_BATCH_SIZES="$(SWEEP_BATCH_SIZES)" POLL_CQ_BUDGETS="$(POLL_CQ_BUDGETS)" bash tests/run_poll_budget_sweep.sh

inlinepollsweep: all
	SWEEP_ITERATIONS=$(SWEEP_ITERATIONS) SWEEP_BATCH_SIZES="$(SWEEP_BATCH_SIZES)" SWEEP_USE_INLINE=1 POLL_CQ_BUDGETS="$(POLL_CQ_BUDGETS)" bash tests/run_poll_budget_sweep.sh

sweepsummary:
	bash tests/summarize_sweep.sh

inlinesweepsummary:
	SWEEP_USE_INLINE=1 bash tests/summarize_sweep.sh

selectivesweepsummary:
	SWEEP_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL) bash tests/summarize_sweep.sh

selectiveinlinesweepsummary:
	SWEEP_USE_INLINE=1 SWEEP_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL) bash tests/summarize_sweep.sh

signalsummary:
	bash tests/summarize_signal_interval_sweep.sh
	bash tests/check_signal_interval_summary.sh

inlinesignalsummary:
	SWEEP_USE_INLINE=1 bash tests/summarize_signal_interval_sweep.sh
	SWEEP_USE_INLINE=1 bash tests/check_signal_interval_summary.sh

pollsummary:
	bash tests/summarize_poll_budget_sweep.sh
	bash tests/check_poll_budget_summary.sh

inlinepollsummary:
	SWEEP_USE_INLINE=1 bash tests/summarize_poll_budget_sweep.sh
	SWEEP_USE_INLINE=1 bash tests/check_poll_budget_summary.sh

sweepreport: sweep sweepsummary

inlinesweepreport: inlinesweep inlinesweepsummary

selectivesweepreport: selectivesweep selectivesweepsummary

selectiveinlinesweepreport: selectiveinlinesweep selectiveinlinesweepsummary

signalreport: signalsweep signalsummary

inlinesignalreport: inlinesignalsweep inlinesignalsummary

pollreport: pollsweep pollsummary

inlinepollreport: inlinepollsweep inlinepollsummary

comparesweeps:
	bash tests/compare_sweep_modes.sh

comparecheck:
	bash tests/check_compare_report.sh

compareselective:
	SELECTIVE_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL) bash tests/compare_selective_modes.sh

compareselectivecheck:
	SELECTIVE_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL) bash tests/check_compare_selective_report.sh

compareinlineselective:
	SELECTIVE_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL) bash tests/compare_inline_selective_modes.sh

compareinlineselectivecheck:
	SELECTIVE_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL) bash tests/check_compare_inline_selective_report.sh

batchphaseclose: envcheck sweepreport inlinesweepreport comparesweeps comparecheck

selectivephaseclose: envcheck selectivesweepreport compareselective compareselectivecheck

selectiveinlinephaseclose: envcheck selectiveinlinesweepreport compareinlineselective compareinlineselectivecheck

pollphaseclose: envcheck pollreport inlinepollreport

finalphaseclose: envcheck batchphaseclose selectivephaseclose selectiveinlinephaseclose signalreport inlinesignalreport pollphaseclose

dual-server: all
	bash tests/dual_perf_server.sh

dual-client: all
	bash tests/dual_perf_client.sh

envcheck:
	bash tests/check_env.sh

help:
	@printf '%s\n' \
	  'Targets:' \
	  '  make test        - run default smoke test (script defaults)' \
	  '  make quicktest   - run small sample smoke test' \
	  '  make quickcheck  - quicktest + marker summary' \
	  '  make quickdiag   - envcheck + quickcheck' \
	  '  make inlinequickreport - envcheck + inline quick test + CSV export' \
	  '  make selectivequickreport - envcheck + selective quick test + CSV export' \
	  '  make selectiveinlinequickreport - envcheck + inline+selective quick test + CSV export' \
	  '  make fulltest    - run phase-closing smoke test' \
	  '  make fullcheck   - fulltest + marker summary' \
	  '  make fulldiag    - envcheck + fullcheck' \
	  '  make inlinefullreport  - envcheck + inline full test + CSV export' \
	  '  make selectivefullreport - envcheck + selective full test + CSV export' \
	  '  make selectiveinlinefullreport - envcheck + inline+selective full test + CSV export' \
	  '  make markers     - print key result markers from logs' \
	  '  make report      - export perf summary CSV from client log' \
	  '  make quickreport - quickdiag + CSV export' \
	  '  make fullreport  - fulldiag + CSV export' \
	  '  make sweep       - run batch-size sweep and export sweep CSV' \
	  '  make inlinesweep - run inline batch-size sweep and export sweep CSV' \
	  '  make selectivesweep - run selective-signaling sweep and export sweep CSV' \
	  '  make selectiveinlinesweep - run inline+selective sweep and export sweep CSV' \
	  '  make signalsweep - run signal-interval sweep for normal mode' \
	  '  make inlinesignalsweep - run signal-interval sweep for inline mode' \
	  '  make pollsweep - run poll-budget sweep for normal mode' \
	  '  make inlinepollsweep - run poll-budget sweep for inline mode' \
	  '  make sweepsummary - summarize sweep CSV into markdown' \
	  '  make inlinesweepsummary - summarize inline sweep CSV into markdown' \
	  '  make selectivesweepsummary - summarize selective sweep CSV into markdown' \
	  '  make selectiveinlinesweepsummary - summarize inline+selective sweep CSV into markdown' \
	  '  make signalsummary - summarize signal-interval sweep for normal mode' \
	  '  make inlinesignalsummary - summarize signal-interval sweep for inline mode' \
	  '  make pollsummary - summarize poll-budget sweep for normal mode' \
	  '  make inlinepollsummary - summarize poll-budget sweep for inline mode' \
	  '  make sweepreport - sweep + markdown summary' \
	  '  make inlinesweepreport - inline sweep + markdown summary' \
	  '  make selectivesweepreport - selective sweep + markdown summary' \
	  '  make selectiveinlinesweepreport - inline+selective sweep + markdown summary' \
	  '  make signalreport - signal-interval sweep + summary for normal mode' \
	  '  make inlinesignalreport - signal-interval sweep + summary for inline mode' \
	  '  make pollreport - poll-budget sweep + summary for normal mode' \
	  '  make inlinepollreport - poll-budget sweep + summary for inline mode' \
	  '  make comparesweeps - compare normal and inline sweep outputs' \
	  '  make compareselective - compare all-signaled and selective sweep outputs' \
	  '  make compareinlineselective - compare inline and inline+selective sweep outputs' \
	  '  make comparecheck - verify compare report structure and inputs' \
	  '  make compareselectivecheck - verify selective compare report structure and inputs' \
	  '  make compareinlineselectivecheck - verify inline+selective compare report structure and inputs' \
	  '  make batchphaseclose - envcheck + normal/inline sweep + compare + verify' \
	  '  make selectivephaseclose - envcheck + selective sweep + compare + verify' \
	  '  make selectiveinlinephaseclose - envcheck + inline+selective sweep + compare + verify' \
	  '  make pollphaseclose - envcheck + normal/inline poll sweep + verify' \
	  '  make finalphaseclose - run all current project phase-close targets' \
	  '  make dual-server - prepare dual-host server side perf run (ens33/gid-index=1)' \
	  '  make dual-client - prepare dual-host client side perf run (server defaults to 192.168.65.135)' \
	  '  make envcheck    - print RXE/GID/IPv6 environment summary' \
	  'Variables:' \
	  '  QUICK_ITERATIONS=$(QUICK_ITERATIONS) QUICK_BATCH_SIZE=$(QUICK_BATCH_SIZE)' \
	  '  FULL_ITERATIONS=$(FULL_ITERATIONS) FULL_BATCH_SIZE=$(FULL_BATCH_SIZE)' \
	  '  SELECTIVE_SIGNAL_INTERVAL=$(SELECTIVE_SIGNAL_INTERVAL)' \
	  '  SIGNAL_INTERVALS=$(SIGNAL_INTERVALS)' \
	  '  POLL_CQ_BUDGETS=$(POLL_CQ_BUDGETS)' \
	  '  SWEEP_ITERATIONS=$(SWEEP_ITERATIONS) SWEEP_BATCH_SIZES=$(SWEEP_BATCH_SIZES)' \
	  '  PERF_SERVER_CPUSET=<cpu-list> PERF_CLIENT_CPUSET=<cpu-list>' \
	  '  PERF_SERVER_NUMA_NODE=<node> PERF_CLIENT_NUMA_NODE=<node>'

clean:
	rm -rf build \
	  tests/*.log \
	  tests/perf-summary.csv \
	  tests/perf-sweep*.csv \
	  tests/perf-sweep*.md \
	  tests/perf-signal-interval*.csv \
	  tests/perf-signal-interval*.md \
	  tests/perf-poll-budget*.csv \
	  tests/perf-poll-budget*.md \
	  tests/perf-inline-vs-normal-summary.md \
	  tests/perf-selective-vs-all-summary.md \
	  tests/perf-inline-selective-vs-inline-summary.md \
	  tests/sweep*
