# day31 DIRECTORY_TREE

```text
day31/
├── DIRECTORY_TREE.md
├── Makefile
├── README.md
├── START_HERE.md
├── docs/
│   ├── 01_plan.md
│   ├── 01_LOCAL_RUNBOOK.md
│   ├── 02_acceptance.md
│   ├── 03_BENCH_DESIGN.md
│   ├── 04_METRICS_AND_REPORTING.md
│   ├── 05_DAY31_DESIGN_AND_RISK.md
│   └── 06_TROUBLESHOOTING.md
├── driver/
│   ├── Makefile
│   ├── day31_edu_bench.c
│   └── include/
│       └── day31_edu_bench.h
├── env/
│   ├── day31.env
│   └── local.example.env
├── guest/
│   └── init.day31
├── include/
│   └── day31_edu_uapi.h
├── output/
│   ├── .gitkeep
│   ├── day31_bench_report_template.csv
│   ├── day31_bench_summary.md
│   └── day31_quick_commands.md
├── records/
│   ├── .gitkeep
│   └── README.md
├── scripts/
│   ├── 00_check_env.sh
│   ├── 02_build_guest_lspci.sh
│   ├── 03_build_tools.sh
│   ├── 04_prepare_rootfs.sh
│   ├── 05_prepare_runtime_dir.sh
│   ├── 06_run_qemu_day31.sh
│   ├── 07_run_all.sh
│   ├── 08_extract_records.sh
│   ├── 09_build_day31_module.sh
│   ├── 10_prepare_kernel_module_tree.sh
│   ├── 99_clean.sh
│   └── common.sh
├── third_party/
│   └── README.md
├── tools/
│   └── day31_edu_bench_tool.c
└── workdir/
```

## 目录职责

- `driver/`：Day31 内核模块，提供 coherent DMA、`mmap`、`ioctl` 和最小运行统计。
- `tools/`：guest bench 工具，负责实际计时、分位数计算和 CPU 占用统计。
- `guest/`：initramfs 里的 `/init`，负责自动化加载驱动并输出 markers。
- `scripts/`：宿主机一键构建、运行和 records 归档。
- `docs/`：Day31 的任务分析、bench 设计、统计口径、风险与排障文档。
- `records/`：每次真实运行后的原始证据与汇总摘要。
- `output/`：可直接拿去整理或复盘的模板文件。
