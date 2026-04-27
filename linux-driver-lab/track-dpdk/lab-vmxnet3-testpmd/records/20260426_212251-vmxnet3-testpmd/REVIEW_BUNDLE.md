# REVIEW_BUNDLE

## Lab

lab-vmxnet3-testpmd

## Record directory

`/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd/records/20260426_212251-vmxnet3-testpmd`

## Expected test machine

| Item | Value |
|------|-------|
| Guest | Ubuntu 22.04.5 Desktop |
| Kernel | Linux 6.8.0-110-generic |
| Management NIC | ens33 / e1000 |
| DPDK NIC | ens192 / vmxnet3 |
| DPDK PCI | 0000:0b:00.0 |
| DPDK Driver | vfio-pci |

## Evidence checklist

| Evidence | Status |
|----------|--------|
| ENV_CHECK.txt | DONE |
| HUGEPAGE_SETUP.txt | DONE |
| BIND_BEFORE.txt | DONE |
| BIND_AFTER.txt | DONE |
| TESTPMD.log | DONE |
| BIND_STATUS.txt | DONE |
| HUGEPAGE_STATUS.txt | DONE |
| PCI_DETAIL.txt | DONE |
| DMESG_DPDK_NET.txt | DONE |

## Review questions

1. ens33 是否保持管理链路？
2. ens192/0000:0b:00.0 是否为本轮唯一 DPDK 测试口？
3. hugepage 是否有可用页？
4. bind 前后 driver 是否符合预期？
5. testpmd 是否完成 EAL/PMD/port/stats 输出？
6. 失败项是否有完整日志？

## Next

通过后进入：

`track-dpdk/lab-vhost-user-basic`
