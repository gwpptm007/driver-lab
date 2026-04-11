# day30 DIRECTORY_TREE

```text
day30/
├── DIRECTORY_TREE.md
├── Makefile
├── README.md
├── START_HERE.md
├── docs/
│   ├── 01_plan.md
│   ├── 01_LOCAL_RUNBOOK.md
│   ├── 02_acceptance.md
│   ├── 03_MMAP_DESIGN.md
│   ├── 04_USER_KERNEL_DATA_FLOW.md
│   ├── 05_DAY30_DESIGN_AND_RISK.md
│   └── 06_TROUBLESHOOTING.md
├── driver/
│   ├── Makefile
│   ├── day30_edu_mmap.c
│   └── include/
│       └── day30_edu_mmap.h
├── env/
│   ├── day30.env
│   └── local.example.env
├── guest/
│   └── init.day30
├── include/
│   └── day30_edu_uapi.h
├── output/
│   ├── .gitkeep
│   ├── day30_mmap_notes.md
│   └── day30_quick_commands.md
├── records/
│   ├── .gitkeep
│   └── README.md
├── scripts/
│   ├── 00_check_env.sh
│   ├── 02_build_guest_lspci.sh
│   ├── 03_build_tools.sh
│   ├── 04_prepare_rootfs.sh
│   ├── 05_prepare_runtime_dir.sh
│   ├── 06_run_qemu_day30.sh
│   ├── 07_run_all.sh
│   ├── 08_extract_records.sh
│   ├── 09_build_day30_module.sh
│   ├── 10_prepare_kernel_module_tree.sh
│   ├── 99_clean.sh
│   └── common.sh
├── third_party/
│   └── README.md
├── tools/
│   └── day30_edu_mmap_tool.c
└── workdir/
```

## 目录职责

- `driver/`：day30 内核模块，实现 coherent DMA + `mmap` + ioctl。
- `tools/`：guest 用户态工具，通过 `mmap()` 和 ioctl 完成零拷贝验证。
- `guest/`：initramfs 里的 `/init`，负责自动加载驱动并留证。
- `scripts/`：宿主机侧的一键构建、运行和 records 归档脚本。
- `docs/`：day30 的任务分析、设计、风险与排障文档。
- `records/`：每次真实运行后沉淀的原始证据。
- `output/`：对外复盘时可直接引用的简版说明与命令清单。
