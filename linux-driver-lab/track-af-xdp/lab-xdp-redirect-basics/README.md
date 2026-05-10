# lab-xdp-redirect-basics

> AF_XDP track 第一站：先把 XDP attach、动作控制、map stats、XSKMAP/redirect 模型跑通。

## 这一站解决什么

在正式写 AF_XDP socket 之前，需要先确认测试机具备最基本的 XDP 能力：

```text
clang 编译 BPF
    ↓
libbpf loader 加载 BPF object
    ↓
XDP attach 到网卡
    ↓
XDP_PASS / XDP_DROP 动作可控
    ↓
stats map 能看到包数/字节数
    ↓
理解后续 AF_XDP socket 如何通过 XSKMAP 接入
```

## 当前测试机默认值

默认沿用前面 VMware 测试机：

```text
管理网卡：ens33，不允许做 DROP/REDIRECT 实验
测试网卡：ens192 / vmxnet3 / 0000:0b:00.0
```

如果你之前做 DPDK 时把 `ens192` 绑到了 `uio_pci_generic`，需要先把 PCI 设备切回 `vmxnet3` 内核驱动。本 lab 提供了 `02_prepare_kernel_netdev.sh`，但默认不会强制改绑定，必须显式确认。

## 推荐执行

```bash
cd track-af-xdp/lab-xdp-redirect-basics

./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_xdp_pass.sh
sudo AF_XDP_CONFIRM_DROP=YES ./scripts/04_run_xdp_drop.sh
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```

`XDP_REDIRECT` dry-run 是可选项，因为还没有 AF_XDP socket 写入 XSKMAP：

```bash
sudo AF_XDP_CONFIRM_REDIRECT=YES ./scripts/05_run_xdp_redirect_dryrun.sh
```

## 通过标准

最低通过：

```text
PASS_BASIC：
- BPF object 编译成功
- loader 编译成功
- XDP_PASS attach 成功
- stats map 能打印
- detach 正常
```

增强通过：

```text
PASS_ACTION：
- XDP_DROP 显式确认后可运行
- drop action stats 非 0 或日志证明动作执行
```

redirect 只作为模型验证：

```text
REDIRECT_MODEL_READY：
- BPF 程序包含 XSKMAP
- loader 支持 action=redirect
- 文档说明没有 AF_XDP socket 时不能判定 PASS_AF_XDP
```
