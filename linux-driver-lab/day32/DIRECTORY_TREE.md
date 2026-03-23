day32/
├── DIRECTORY_TREE.md
├── Makefile
├── README.md
├── START_HERE.md
├── docs/
│   ├── 01_LOCAL_RUNBOOK.md
│   ├── 01_plan.md
│   ├── 02_acceptance.md
│   ├── 03_BENCH_DESIGN.md
│   ├── 04_METRICS_AND_REPORTING.md
│   ├── 05_DAY32_DESIGN_AND_RISK.md
│   ├── 06_TROUBLESHOOTING.md
│   └── 07_TEST_RESULT_ANALYSIS.md
├── driver/
│   ├── Makefile
│   ├── day32_edu_perf.c
│   └── include/day32_edu_perf.h
├── env/
│   ├── day32.env
│   ├── local.example.env
│   └── local_wq7.env
├── guest/
│   └── init.day32
├── include/
│   └── day32_edu_uapi.h
├── output/
│   ├── .gitkeep
│   ├── day32_bench_report_template.csv
│   ├── day32_bench_summary.md
│   ├── day32_perf_report_template.md
│   ├── day32_perf_summary.md
│   └── day32_quick_commands.md
├── records/
│   ├── .gitkeep
│   └── README.md
├── scripts/
│   ├── 00_check_env.sh
│   ├── 01_fetch_pciutils.sh
│   ├── 02_build_guest_lspci.sh
│   ├── 03_build_tools.sh
│   ├── 04_prepare_rootfs.sh
│   ├── 05_prepare_runtime_dir.sh
│   ├── 06_run_qemu_day32.sh
│   ├── 07_run_all.sh
│   ├── 08_extract_records.sh
│   ├── 09_build_day32_module.sh
│   ├── 10_prepare_kernel_module_tree.sh
│   ├── 11_collect_perf_baseline.sh
│   ├── 12_collect_perf_optimized.sh
│   ├── 13_compare_perf_reports.sh
│   ├── 99_clean.sh
│   └── common.sh
├── third_party/
│   └── README.md
├── tools/
│   └── day32_edu_perf_tool.c
└── workdir/
