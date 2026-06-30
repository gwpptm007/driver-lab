# lab-dpdk-vfio-iommu-boundary

Phase 4：UIO / VFIO / IOMMU / MSI-X 环境边界。

## 目标

把当前测试机上的 DPDK 设备接管边界讲清楚：

- UIO 和 VFIO 的差异。
- VFIO 对 IOMMU group 的依赖。
- vmxnet3 / VMware 环境边界。
- 管理网卡不能随意 bind/unbind。

## 状态

```text
PASS_VFIO_IOMMU_BOUNDARY
```

正式记录：

```text
records/20260629-212638-vfio-iommu/
```

## 快速复测

```bash
cd linux-driver-lab/track-dpdk-advanced/lab-dpdk-vfio-iommu-boundary
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-vfio-iommu"
./scripts/00_collect_boundary.sh
./scripts/01_collect_vmxnet3_context.sh
./scripts/02_generate_summary.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 文档入口

- `docs/01_OVERVIEW.md`
- `docs/02_TEST_AND_VERIFY.md`
- `docs/03_RESULT_ANALYSIS.md`
- `docs/04_DEEP_LEARNING.md`

## 边界

本 lab 不强行切换网卡 driver，不自动 bind/unbind 管理口，也不修改 GRUB。它只收集当前状态并形成可信边界文档。

