# 02_TEST_AND_VERIFY - 娴嬭瘯鍛戒护涓庢墽琛岃褰?
## 娴嬭瘯璁板綍

```text
records/20260629-212638-vfio-iommu/
```

## 瀹屾暣鍛戒护

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/lab-dpdk-vfio-iommu-boundary
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-vfio-iommu"
./scripts/00_collect_boundary.sh
./scripts/01_collect_vmxnet3_context.sh
./scripts/02_generate_summary.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 鐢熸垚鏂囦欢

```text
BOUNDARY_ENV.log
VMXNET3_CONTEXT.log
SUMMARY.md
```

## 鍏抽敭杈撳嚭

```text
cmdline=BOOT_IMAGE=... ro quiet splash
iommu_group_entries=0
vfio_module_loaded=no
uio_module_loaded=no
vmxnet3_line=Kernel driver in use: vmxnet3
```

## 楠屾敹瑙ｉ噴

| 椤?| 缁撴灉 | 瑙ｉ噴 |
|---|---|---|
| `PASS_UIO_VFIO_MATRIX` | PASS | UIO/VFIO/IOMMU 宸紓宸叉垚鐭╅樀 |
| `PASS_VMXNET3_BOUNDARY` | PASS | vmxnet3 褰撳墠鐘舵€佸拰杈圭晫宸茶褰?|
| `PASS_IOMMU_CHECKLIST` | PASS | VFIO 鍓嶇疆鏉′欢 checklist 宸插舰鎴?|

## 娉ㄦ剰

杩欎釜闃舵娌℃湁鎵ц鍗遍櫓鐨勭湡瀹?NIC bind/unbind銆?
鍘熷洜锛?
```text
褰撳墠鏈哄櫒渚濊禆 SSH 绠＄悊缃戝崱锛?IOMMU group 涓虹┖锛?璐哥劧 bind VFIO 鏃㈠彲鑳芥柇杩烇紝涔熶笉鑳借瘉鏄?VFIO 闅旂鑳藉姏銆?```
