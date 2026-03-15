# day22 DIRECTORY_TREE

```text
day22/
├── README.md
├── START_HERE.md
├── DIRECTORY_TREE.md
├── Makefile
├── env/
│   └── day22.env
├── scripts/
│   ├── common.sh
│   ├── 00_check_host_tools.sh
│   ├── 01_check_kernel_config.sh
│   ├── 02_build_guest_tools.sh
│   ├── 02_build_guest_lspci.sh
│   ├── 03_prepare_rootfs.sh
│   ├── 04_start_ivshmem_server.sh
│   ├── 05_run_qemu_ivshmem.sh
│   ├── 06_extract_records.sh
│   ├── 07_run_all.sh
│   ├── 08_clean.sh
│   └── 09_build_stub_module.sh
├── tools/
│   ├── Makefile
│   └── pci_sysfs_dump.c
├── driver/
│   ├── Makefile
│   ├── day22_ivshmem_stub.c
│   └── include/
│       └── day22_ivshmem_stub.h
├── guest/
│   └── init.day22
├── docs/
│   ├── 01_day22_goal_and_scope.md
│   ├── 02_day22_architecture.md
│   ├── 03_platform_prep.md
│   ├── 04_step_by_step_test.md
│   ├── 05_troubleshooting.md
│   └── 06_acceptance.md
├── output/
│   ├── day22_expected_artifacts.md
│   ├── day22_manual_test_commands.md
│   └── day22_test_checklist.md
├── records/
│   └── README.md
├── third_party/
│   └── README.md
└── workdir/
    └── .gitkeep
```

## 目录设计说明

### 1. `tools/`

这里放 day22 自己的 guest 侧 C 工具。

这次最关键的是 `pci_sysfs_dump.c`：

- 它运行在 guest 里
- 它直接读 `/sys/bus/pci/devices`
- 它是 day22 “补回 C 代码味”的第一块核心代码

### 2. `driver/`

这里放 day22 自己的内核模块骨架。

当前先提供：

- 设备 ID 匹配
- `pci_driver` 注册
- `probe/remove`
- 私有结构体
- BAR 资源打印

真正的 `pci_enable_device/request_regions/pci_iomap` 留给 day23。

### 3. `scripts/`

所有 day22 自动化逻辑都收口在这里，不依赖前面 day 的脚本。

### 4. `guest/`

放 guest 里执行的脚本模板。day22 启动后会自动执行：

- `lspci`
- `pci_sysfs_dump`
- `dmesg` 采集
- sysfs/config 证据采集

### 5. `records/`

只放真实运行证据，不放说明文档。

### 6. `output/`

放检查表、模板、手工命令单。
