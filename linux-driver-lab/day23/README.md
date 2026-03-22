# day23：ivshmem `pci_driver` 骨架最终版

## 1. 最终结论

**day23 已通过。**

这一天的目标不是做 MMIO 协议、MSI 或用户态接口，而是把 `pci_driver` 真正接到 `ivshmem (1af4:1110)` 上，完成：

- 外部模块构建
- guest 内 `insmod` / `rmmod`
- `probe()` 进入
- BAR0 / BAR2 资源识别
- `pci_iomap()` 基础映射
- `remove()` 对称释放

本轮真实验证已经证明：

- `ivshmem` 设备可以在 guest 内被枚举到
- `day23_ivshmem_probe.ko` 可以成功加载
- 驱动能打印 `vendor/device/class/irq`
- BAR0 / BAR2 的 start/end/len/flags 被正确打印
- BAR0 / BAR2 的 `pci_iomap()` 成功
- `BAR0 first dword` 已打印
- `remove()` 成功执行并退出

## 2. day23 的定位

- `day22`：证明 PCI 设备能被 guest 枚举到
- `day23`：证明最小 `pci_driver` 能真正接住设备
- `day24`：在 `probe()` 成功基础上继续做 MMIO 读写闭环

## 3. 最少只看哪些文件

1. `START_HERE.md`
2. `env/local.wq7.env`
3. `docs/01_LOCAL_RUNBOOK.md`
4. `docs/02_RESULTS_AND_ACCEPTANCE.md`
5. `driver/day23_ivshmem_probe.c`

## 4. 本轮真实跑通后的验收摘要

本轮真实验证中，`run-summary.md` 已是全 yes：

- insmod 成功：yes
- probe 成功：yes
- BAR0 信息：yes
- BAR2 信息：yes
- rmmod 成功：yes
- guest 流程完成：yes

串口日志中也已经出现：

- `probe enter: vendor=1af4 device=1110 class=0x050000 irq=0`
- `BAR0: start=0x0000000010081000 ... len=0x100`
- `BAR2: start=0x0000008000000000 ... len=0x400000`
- `BAR0 mapped`
- `BAR2 mapped`
- `BAR0 first dword=0x00000000`
- `probe success`
- `remove enter`
- `remove leave`
- `===DAY23:COMPLETE===`

## 5. 当前推荐执行顺序

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day23
source env/local.wq7.env

make check
make kernel-module-tree
make module
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run
```

## 6. 当前已知非阻塞项

`qemu.stderr.log` 里可能仍会看到 QEMU 对 `share` 短格式参数的弃用告警。它不影响 day23 功能正确性，后续有时间再把参数改成 `share=on` 即可。
