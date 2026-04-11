# day23 DIRECTORY_TREE

```text
day23/
├── README.md
├── START_HERE.md
├── DIRECTORY_TREE.md
├── Makefile
├── env/
│   ├── day23.env
│   ├── local.example.env
│   └── local.wq7.env
├── driver/
│   ├── Makefile
│   ├── day23_ivshmem_probe.c
│   └── include/
│       └── day23_ivshmem_probe.h
├── guest/
│   └── init.day23
├── scripts/
│   ├── 00_check_host_tools.sh
│   ├── 00_discover_common_paths.sh
│   ├── 01_check_kernel_config.sh
│   ├── 02_build_guest_lspci.sh
│   ├── 03_prepare_rootfs.sh
│   ├── 04_prepare_ivshmem_backend.sh
│   ├── 05_run_qemu_day23.sh
│   ├── 06_extract_records.sh
│   ├── 07_run_all.sh
│   ├── 08_clean.sh
│   ├── 09_build_day23_module.sh
│   ├── 10_prepare_kernel_module_tree.sh
│   └── common.sh
├── docs/
│   ├── 01_LOCAL_RUNBOOK.md
│   ├── 02_ACCEPTANCE.md
│   └── 03_TROUBLESHOOTING.md
├── output/
│   ├── .gitkeep
│   └── day23_quick_commands.md
├── records/
│   ├── .gitkeep
│   └── README.md
└── third_party/
    └── README.md
```
