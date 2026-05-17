# scripts/ - lab-dpdk-l2-forwarding 脚本说明

所有脚本共享 [common.sh](common.sh) 中的环境变量和辅助函数。

## 执行顺序

```text
00 → 01 → 02 → 03 (或 04 / 05) → 06 → 07
                                        ↑
                                    08 随时可跑
```

## 脚本一览

| 脚本 | 需要 root | 改系统状态 | 作用 |
|------|-----------|------------|------|
| [00_check_env.sh](00_check_env.sh) | 否 | 否 | 收集环境快照（OS/kernel/工具链/libdpdk/PCI/hugepage） |
| [01_build_app.sh](01_build_app.sh) | 否 | 否 | meson setup + ninja 编译 l2fwd-lite |
| [02_prepare_vmxnet3.sh](02_prepare_vmxnet3.sh) | **是** | **是** | 挂载 hugetlbfs、分配 hugepage、加载 uio_pci_generic、绑定 0000:0b:00.0 |
| [03_run_l2fwd_single_port.sh](03_run_l2fwd_single_port.sh) | **是** | 否 | 单端口 smoke：`-a 0000:0b:00.0`，RX/free 模式 |
| [04_run_l2fwd_two_port.sh](04_run_l2fwd_two_port.sh) | **是** | 否 | 双端口转发：需要 `DPDK_PCI_1=0000:xx:yy.z`，两端口配对 L2 交换 |
| [05_run_l2fwd_vdev_null_pair.sh](05_run_l2fwd_vdev_null_pair.sh) | 否 | 否 | 虚拟双端口：`--no-pci --vdev net_null0 --vdev net_null1`，不依赖物理网卡 |
| [06_collect_stats.sh](06_collect_stats.sh) | 否 | 否 | 汇总 hugepage/网卡/日志关键行到 COLLECT_STATS.txt |
| [07_make_review_bundle.sh](07_make_review_bundle.sh) | 否 | 否 | 生成 REVIEW_BUNDLE.md（文件清单 + 关键证据 + PASS 判定标准） |
| [08_clean_runtime.sh](08_clean_runtime.sh) | 否 | 否 | pkill 残留 l2fwd-lite 进程（不卸载驱动、不恢复绑定） |

## 关键 EAL 参数（由 common.sh 控制）

```text
-l 0-1                        使用 lcore 0 和 1
-n 4                          4 个内存通道
--file-prefix l2fwd_lite      DPDK 文件前缀（多实例隔离）
-a 0000:0b:00.0               绑定 VMXNET3 PCI 设备
```

可通过环境变量覆盖，例如：

```bash
sudo L2FWD_RUN_SECONDS=30 L2FWD_BURST_SIZE=64 ./scripts/03_run_l2fwd_single_port.sh
sudo DPDK_PCI_1=0000:13:00.0 ./scripts/04_run_l2fwd_two_port.sh
```

## 产出文件

所有脚本将输出写入 `records/<timestamp>-dpdk-l2-forwarding/` 目录：

```text
ENV_CHECK.txt                  00 环境检查
BUILD.log                      01 编译日志
PREPARE_VMXNET3.txt            02 网卡绑定日志
L2FWD_SINGLE_PORT.log          03 单端口运行日志
L2FWD_TWO_PORT.log             04 双端口运行日志
L2FWD_VDEV_NULL_PAIR.log       05 虚拟双端口运行日志
COLLECT_STATS.txt              06 统计汇总
REVIEW_BUNDLE.md               07 评审包
COMMANDS.md                    所有脚本执行过的原始命令
```
