# 02_TEST_AND_VERIFY - 测试命令与执行记录

## 测试记录

```text
records/20260629-212638-vfio-iommu/
```

## 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/lab-dpdk-vfio-iommu-boundary
export RECORD_DIR="$PWD/records/$(date +%Y%m%d-%H%M%S)-vfio-iommu"
./scripts/00_collect_boundary.sh
./scripts/01_collect_vmxnet3_context.sh
./scripts/02_generate_summary.sh "$RECORD_DIR"
cat "$RECORD_DIR/SUMMARY.md"
```

## 生成文件

```text
BOUNDARY_ENV.log
VMXNET3_CONTEXT.log
SUMMARY.md
```

## 关键输出

```text
cmdline=BOOT_IMAGE=... ro quiet splash
iommu_group_entries=0
vfio_module_loaded=no
uio_module_loaded=no
vmxnet3_line=Kernel driver in use: vmxnet3
```

## 验收解释

| 项 | 结果 | 解释 |
|---|---|---|
| `PASS_UIO_VFIO_MATRIX` | PASS | UIO/VFIO/IOMMU 差异已成矩阵 |
| `PASS_VMXNET3_BOUNDARY` | PASS | vmxnet3 当前状态和边界已记录 |
| `PASS_IOMMU_CHECKLIST` | PASS | VFIO 前置条件 checklist 已形成 |

## 注意

这个阶段没有执行危险的真实 NIC bind/unbind。

原因：

```text
当前机器依赖 SSH 管理网卡；
IOMMU group 为空；
贸然 bind VFIO 既可能断连，也不能证明 VFIO 隔离能力。
```

