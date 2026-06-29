# lab-dpdk-vfio-iommu-boundary

> track-dpdk-advanced Phase 4锛歎IO / VFIO / IOMMU / MSI-X 鐜杈圭晫銆?
## 鐩爣

鎶婂綋鍓嶆祴璇曟満涓婄殑 DPDK 璁惧鎺ョ杈圭晫璁叉竻妤氾細

```text
kernel cmdline
  -> IOMMU status
  -> vfio/uio modules
  -> dpdk-devbind status
  -> vmxnet3 context
  -> UIO/VFIO matrix
  -> checklist
```

## 褰撳墠鐘舵€?
```text
READY_TO_TEST
```

## 楠屾敹椤?
```text
PASS_UIO_VFIO_MATRIX
PASS_VMXNET3_BOUNDARY
PASS_IOMMU_CHECKLIST
```

## 澶嶆祴鍛戒护

```bash
cd linux-driver-lab/track-dpdk-advanced/lab-dpdk-vfio-iommu-boundary
chmod +x scripts/*.sh
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-vfio-iommu"
./scripts/00_collect_boundary.sh
./scripts/01_collect_vmxnet3_context.sh
./scripts/02_generate_summary.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 鏂囨。鍏ュ彛

- `docs/01_OVERVIEW.md`
- `docs/04_DEEP_LEARNING.md`
- `docs/02_TEST_AND_VERIFY.md`
- `docs/03_RESULT_ANALYSIS.md`

## 杈圭晫

鏈?Lab 涓嶅己琛屽垏鎹㈢綉鍗?driver锛屼笉鑷姩 bind/unbind 绠＄悊鍙ｏ紝涔熶笉淇敼 GRUB銆傚彧鏀堕泦褰撳墠鐘舵€佸苟褰㈡垚鍙俊杈圭晫鏂囨。銆?