# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Linux driver learning laboratory with multiple progressive tracks:

| Track | Path | Build | Status |
|-------|------|-------|--------|
| `foundation/` | day01-day35 (W1-W5): miscdevice → platform/IRQ → PCIe → DMA/performance | `build.sh` per day | Complete |
| `netdev/` | stage00-stage14: net_device/skb/NAPI → virtio-net → XDP | `make` per stage | Complete |
| `track-af-xdp/` | AF_XDP 4-phase track: XDP redirect → socket/rings → zero-copy probe → mini forwarder | `make` per lab | **All 4 phases PASS (2026-06-07)** |
| `track-dpdk/` | DPDK user-space fastpath (vmxnet3 PMD → vhost-user → virtio-user → fastpath C app) | `meson + ninja` | Complete |
| `track-real-driver/` | virtio-net source code dive | (analysis only) | — |
| `track-virtual-net/` | tap/bridge/vhost mechanisms | (analysis only) | — |
| `track-ebpf-observability/` | eBPF network path observability | (analysis only) | — |

## Build and Test Commands

### Foundation Days (W1-W5)

```bash
cd linux-driver-lab/foundation/dayXX
chmod +x build.sh
./build.sh
```

`build.sh` executes: driver compile → rootfs prep → QEMU launch. QEMU exit: `Ctrl+a x` (quit) or `Ctrl+a c` then `quit` (monitor).

### Manual Module Build

```bash
make KDIR=/path/to/kernel/build/dir clean
make KDIR=/path/to/kernel/build/dir
```

### Track-DPDK (meson + ninja)

```bash
cd track-dpdk/project-user-space-fastpath
./scripts/01_build_app.sh        # meson compile
sudo ./scripts/03_run_fastpath_single_port.sh  # run
```

### Netdev Stages

```bash
cd linux-driver-lab/netdev/stageXX
make KDIR=/path/to/kernel/build/dir
```

## Environment Dependencies

```
driver-lab/
├── linux-driver-lab/       # This repository
└── kernel-src/             # External (sibling directory)
    ├── linux-5.15.10/
    │   ├── build/x86/      # KDIR for module build
    │   └── output/x86/bzImage  # KERNEL_IMG
    └── busybox-1.36.1/output/x86/_install  # BUSYBOX_INSTALL
```

Key env vars: `KDIR`, `KERNEL_IMG`, `BUSYBOX_INSTALL`.

### ARM64 Cross-compilation (W2, stage06)

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
./build.sh
```

Required: `qemu-system-aarch64`, `dtc`, `aarch64-linux-gnu-gcc`.

## Code Architecture

### Foundation Day Structure

```
dayXX/
├── demo.c                 # Driver source
├── Makefile               # Module build rules
├── build.sh               # Full experiment + QEMU
├── test.c / test_ioctl.c  # Userspace test programs
├── demo_ioctl.h           # Shared ioctl definitions
├── README.md              # Learning goals
├── docs/                  # Detailed documentation
├── records/               # Raw evidence/logs (timestamped)
└── output/                # Processed results
```

### Driver Patterns by Week

| Week | Days | Pattern |
|------|------|---------|
| W1 | 01-07 | `miscdevice` + `misc_register()` |
| W2 | 08-14 | `platform_driver` + Device Tree + `of_property_read_*` |
| W4 | 22-28 | `pci_register_driver` + BAR/MMIO + MSI |
| W5 | 29-35 | `dma_alloc_coherent` + `mmap` + perf/ftrace |

### Netdev Stage Structure

```
stageXX/
├── demo.c                 # Netdev driver source
├── Makefile               # Uses kernel build system
├── test.c                 # Userspace test (optional)
├── README.md              # Stage-specific goals
└── scripts/               # Helper scripts (optional)
```

### Track-DPDK Structure

```
track-dpdk/
├── lab-vmxnet3-testpmd/   # DPDK environment validation
├── lab-vhost-user-basic/  # vhost-user socket experiments
├── lab-virtio-user-vhost/ # virtio-user + vhost-user pairing
├── lab-dpdk-l2-forwarding/# L2fwd C app (meson build)
├── project-user-space-fastpath/  # fastpath C app (meson build)
│   └── app/main.c         # EAL init, mbuf pool, rx_burst/tx_burst, classify/rewrite
└── docs/                  # Consolidated documentation
```

### Device Tree Injection (W2 / stage05-06)

ARM64 experiments use `inject_virt_dt.py`:
1. `qemu-system-aarch64 -machine virt,dumpdtb=virt-base.dtb`
2. `dtc -I dtb -O dts -o virt-base.dts`
3. `python inject_virt_dt.py --input virt-base.dts --fragment demo.fragment.dtsi --output virt-new.dts`
4. `dtc -I dts -O dtb -o virt-new.dtb`

## Key Implementation Details

### Module Unloading Safety

Symmetric resource management: free in reverse order of allocation. Test with `insmod/rmmod` cycles (200-1000 iterations).

### Evidence Collection

From day17/W3 onward:
- `records/`: Raw outputs (dmesg, lspci, trace logs, benchmarks)
- `output/`: Processed summaries, timestamped (`compare-20260314-231137.md`)

### Rootfs Generation

Static-linked BusyBox required. Check: `file busybox` shows "statically linked". Package: `find . | cpio -o -H newc | gzip -9 > ../rootfs.img`

## Documentation Structure (`docs/`)

After consolidation, docs are organized as:

| File | Content |
|------|---------|
| `01_PROGRAMS.md` | Current phase, track positioning, next steps |
| `02_EXPERT_REVIEW.md` | Expert review conclusions and execution plans |
| `03_PROGRESS.md` | Current progress and completion matrix |
| `04_ARCHITECTURE.md` | Architecture layering and completion matrix |
| `05_START_HERE.md` | Quick start and GitHub usage |
| `06_TEST_GUIDE.md` | Test record templates |
| `07_FOUNDATION_REVIEWS.md` | W1-W5 week reviews and plans |

### Recommended Reading Order

1. `linux-driver-lab/README.md` - Overall roadmap
2. `linux-driver-lab/track-af-xdp/README.md` - AF_XDP track (all 4 phases PASS)
3. `foundation/README.md` - Day01-35 learning path
4. `netdev/README.md` - Stage00-14 network driver
5. `track-dpdk/README.md` - DPDK user-space networking
6. `linux-driver-lab/docs/03_PROGRESS.md` - Current progress and open items
