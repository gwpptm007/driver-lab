# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Linux driver learning laboratory organized into 35 days (day01-day35) covering 5 weeks (W1-W5) of progressive driver development:

- **W1 (day01-07)**: Character device basics (miscdevice, ioctl, sysfs, debugfs, waitqueue/workqueue, regression testing)
- **W2 (day08-14)**: Embedded driver patterns (platform_driver, Device Tree, IRQ, regmap, ftrace, bring-up checklist)
- **W3 (day15-21)**: Kernel trimming and porting (defconfig, rootfs, startup chain, regression)
- **W4 (day22-28)**: PCIe fundamentals (ivshmem-doorbell device, PCI enumeration, BAR/MMIO, MSI interrupts, user tools)
- **W5 (day29-35)**: DMA + performance analysis (QEMU EDU device, dma_alloc_coherent, mmap, benchmarking, perf, ftrace, stability)

## Build and Test Commands

### Standard Build Pattern (x86)

Each day directory typically contains a `build.sh` script that performs the complete experiment chain:

```bash
cd linux-driver-lab/dayXX
chmod +x build.sh
./build.sh
```

The build.sh script:
1. Compiles the driver module (`demo.ko`)
2. Compiles user-space test programs if present (`test.c`, `test_ioctl.c`)
3. Prepares minimal rootfs with BusyBox
4. Generates `/init` script
5. Packages `rootfs.img` via `cpio | gzip`
6. Launches QEMU

### Manual Module Build

```bash
make KDIR=/path/to/kernel/build/dir clean
make KDIR=/path/to/kernel/build/dir
```

### QEMU Exit

When QEMU runs with `-nographic` + `console=ttyS0`, exit using:
- `Ctrl+a`, then `x` to quit directly
- `Ctrl+a`, then `c` to enter QEMU monitor, type `quit`

## Environment Dependencies

The repository expects a sibling directory structure:

```
driver-lab/
├── linux-driver-lab/       # This repository
└── kernel-src/
    ├── linux-5.15.10/
    │   ├── build/x86/      # x86 kernel build directory
    │   ├── build/arm64/    # arm64 kernel build directory
    │   └── output/
    │       ├── x86/bzImage
    │       └── arm64/Image
    └── busybox-1.36.1/
        └── output/
            ├── x86/_install/bin/busybox
            └── arm64/_install/bin/busybox
```

Key paths (configurable via environment variables):
- `KDIR`: Kernel build directory (`../kernel-src/linux-5.15.10/build/x86` or `/build/arm64`)
- `KERNEL_IMG`: Kernel image (`../kernel-src/linux-5.15.10/output/x86/bzImage` or `/arm64/Image`)
- `BUSYBOX_INSTALL`: BusyBox installation (`../kernel-src/busybox-1.36.1/output/x86/_install`)

### Cross-compilation (ARM64)

For ARM64 experiments (day08-14), set cross-compiler variables:

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
./build.sh
```

Required tools: `qemu-system-aarch64`, `dtc` (device-tree-compiler), `aarch64-linux-gnu-gcc`

## Code Architecture

### Standard Day Directory Structure

```
dayXX/
├── demo.c                 # Main driver source
├── Makefile               # Module compilation rules
├── build.sh               # Complete experiment script
├── test.c / test_ioctl.c  # User-space test programs
├── demo_ioctl.h           # ioctl command definitions
├── README.md              # Day-specific learning goals and tasks
├── START_HERE.md          # Quick start guide (later weeks)
├── docs/                  # Detailed documentation (later weeks)
├── records/               # Raw evidence/logs (later weeks)
├── output/                # Processed results (later weeks)
└── rootfs/                # Temporary rootfs directory (generated)
```

### Driver Patterns

**Early days (01-07)**: Use `miscdevice` for simple character devices:
- `misc_register()` for device registration
- `struct file_operations` for VFS callbacks
- `/dev/demo` device node created automatically

**W2 (08-14)**: Platform driver model:
- `platform_driver_register()` with `probe`/`remove`
- `of_match_ptr` for Device Tree matching
- `of_property_read_*` for reg/irq parsing
- `request_irq()` with top-half handlers
- `workqueue` for bottom-half deferred work
- `regmap` for register abstraction

**W4 (22-28)**: PCI driver model:
- `pci_register_driver()` with `pci_driver` struct
- `pci_enable_device()`, `pci_request_regions()`, `pci_iomap()`
- BAR resource management and MMIO access
- `pci_alloc_irq_vectors()`, `request_irq()` for MSI
- User tools for device interaction

**W5 (29-35)**: DMA and performance:
- `dma_set_mask_and_coherent()`
- `dma_alloc_coherent()` for DMA buffers
- `mmap()` file operation for zero-copy userspace access
- Benchmarking with throughput/latency metrics
- `perf record/report` for hotspots
- `ftrace function_graph` for call path analysis

## Key Implementation Details

### Device Tree Injection (day08-14)

ARM64 experiments use `inject_virt_dt.py` to inject test device fragments:
1. Dump QEMU virt base DTB: `qemu-system-aarch64 -machine virt,dumpdtb=virt-base.dtb`
2. Decompile: `dtc -I dtb -O dts -o virt-base.dts`
3. Inject fragment: `python inject_virt_dt.py --input virt-base.dts --fragment demo.fragment.dtsi --output virt-new.dts`
4. Recompile: `dtc -I dts -O dtb -o virt-new.dtb`

### Evidence Collection Pattern

From day17 onward, maintain organized evidence:
- `records/`: Raw outputs (dmesg, lspci, trace logs, benchmark results)
- `output/`: Processed summaries and analysis
- Each run gets timestamped records (e.g., `compare-20260314-231137.md`)

### Rootfs Generation

Always use static-linked BusyBox for minimal rootfs. Check with `file` command:
```bash
file path/to/busybox  # Should NOT show "dynamically linked"
```

Rootfs packaging:
```bash
cd rootfs
find . | cpio -o -H newc | gzip -9 > ../rootfs.img
```

## Testing and Regression

### Basic Verification Steps

1. Check module loaded: `lsmod | grep demo`
2. Check device node: `ls -l /dev/demo`
3. Verify functionality: `echo hello > /dev/demo` (varies by day)
4. Check kernel logs: `dmesg | tail`
5. Verify no Oops/warning/panic

### Regression Testing

- Day06 includes regression scripts for insmod/rmmod cycling and concurrent stress testing
- W3 focuses on baseline freezing and automated regression
- Use scripts in each day for standardized testing when available

## Important Development Notes

### Build Script Design

The `build.sh` scripts are designed to be the single entry point for each day's experiment. They:
- Handle relative path resolution to support different repository layouts
- Support environment variable overrides (KDIR, KERNEL_IMG, BUSYBOX_INSTALL)
- Detect and use static-linked BusyBox automatically
- Provide clear error messages with hints for missing dependencies

### Module Unloading Safety

Always implement symmetric resource management:
- What's allocated in `probe/init` must be freed in `remove/exit`
- Order: free in reverse order of allocation
- Test with `insmod/rmmod` cycles (200-1000 iterations) for stability

### Code Organization

- Driver source: `demo.c` or descriptive names (`demo_pdrv.c`, `demo_irq.c`)
- ioctl definitions: `demo_ioctl.h` shared with userspace test programs
- Test programs: compile with `gcc -static` for minimal rootfs compatibility

### Reading Order for New Users

For understanding the project structure and learning path:
1. `linux-driver-lab/README.md` - Overall roadmap
2. `docs/FILE_GUIDE.md` - File purpose explanation
3. `docs/ROADMAP.md` - Week-by-week learning goals
4. Individual day `README.md` files for specific topics
5. Later weeks: `START_HERE.md` → `docs/01_overview.md` etc.

### W4-W5 Integration

W4 (PCIe basics with ivshmem-doorbell) and W5 (DMA/performance with QEMU EDU) are designed as complementary:
- W4: PCI enumeration, BAR/MMIO, MSI interrupts, remove symmetry
- W5: DMA coherent buffers, mmap zero-copy, benchmarking, perf/ftrace analysis, stability

The evidence collection pattern from W3 should be carried forward into W4/W5.
