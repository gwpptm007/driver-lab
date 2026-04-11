# Day18 目录树

```text
.
├── DIRECTORY_TREE.md
├── FIRST_RUN.md
├── Makefile
├── README.md
├── START_HERE.md
├── apply_config.sh
├── baseline_template.csv
├── build.sh
├── build_perf.sh
├── check_profile_equivalence.sh
├── check_round_profiles.sh
├── collect
│   ├── guest_collect.sh
│   ├── host_collect.sh
│   └── parse_meminfo.awk
├── compare_results.py
├── config
│   ├── 10_required.fragment
│   ├── 20_platform.fragment
│   ├── 30_debug.fragment
│   ├── 40_perf.fragment
│   ├── 90_trim_day18.fragment
│   ├── category_manifest.csv
│   ├── trace_baseline.fragment
│   ├── trim_round1.fragment
│   └── trim_round2b.fragment
├── demo_regmap.c
├── demo_regmap.fragment.dtsi
├── docs
│   ├── 01_day18_requirement_analysis.md
│   ├── 02_category_matrix.md
│   ├── 03_day18_technical_route.md
│   ├── 04_day18_profile_design.md
│   ├── 05_day18_learning_points.md
│   ├── 06_day18_acceptance_and_next_steps.md
│   ├── 07_execution_steps_and_validation.md
│   ├── 08_perf_build_analysis.md
│   ├── 11_records_detailed_interpretation.md
│   └── 12_day18_final_acceptance.md
├── export_category_view.py
├── function_graph_targets.txt
├── inject_virt_dt.py
├── notes_code_layout.md
├── output
│   └── .gitkeep
├── records
│   └── .gitkeep
├── rootfs
├── rootfs.img
├── run_compare_profiles.sh
├── run_compare_rounds.sh
├── run_profile_collect.sh
└── run_qemu.sh
```

说明：

- `rootfs/`、`rootfs.img`、`output/perf/` 这些属于运行后逐步生成的实验产物。
- 你打包时如果希望更轻，可以只保留源码、脚本、文档和 `records/`，把临时构建产物去掉。
- 当前 Day18 的核心是“目录完全独立”，所以配置、脚本、文档、采样、对比都只放在 `day18/` 内。
