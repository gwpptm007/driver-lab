# day33 目录说明

```text
 day33/
 ├── README.md
 ├── START_HERE.md
 ├── Makefile
 ├── DIRECTORY_TREE.md
 ├── docs/
 │   ├── 01_LOCAL_RUNBOOK.md
 │   ├── 01_plan.md
 │   ├── 02_acceptance.md
 │   ├── 03_FUNCTION_GRAPH_DESIGN.md
 │   ├── 04_TRACE_READING_GUIDE.md
 │   ├── 05_DAY33_DESIGN_AND_RISK.md
 │   ├── 06_TROUBLESHOOTING.md
 │   └── 07_TEST_RESULT_ANALYSIS.md
 ├── driver/
 │   ├── Makefile
 │   ├── day33_edu_trace.c
 │   └── include/day33_edu_trace.h
 ├── include/
 │   └── day33_edu_uapi.h
 ├── tools/
 │   └── day33_edu_trace_tool.c
 ├── guest/
 │   └── init.day33
 ├── env/
 │   ├── day33.env
 │   ├── local.example.env
 │   └── local_wq7.env
 ├── scripts/
 │   ├── 00_check_env.sh
 │   ├── 01_fetch_pciutils.sh
 │   ├── 02_build_guest_lspci.sh
 │   ├── 03_build_tools.sh
 │   ├── 04_prepare_rootfs.sh
 │   ├── 05_prepare_runtime_dir.sh
 │   ├── 06_run_qemu_day33.sh
 │   ├── 07_run_all.sh
 │   ├── 08_extract_records.sh
 │   ├── 09_build_day33_module.sh
 │   ├── 10_prepare_kernel_module_tree.sh
 │   ├── 11_generate_trace_summary.sh
 │   ├── 99_clean.sh
 │   └── common.sh
 ├── output/
 │   ├── day33_ftrace_explain_template.md
 │   ├── day33_trace_summary.md
 │   └── day33_trace_quick_commands.md
 ├── records/
 │   └── README.md
 ├── third_party/
 │   └── README.md
 └── workdir/
```
