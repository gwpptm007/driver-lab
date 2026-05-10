# START_HERE

## 先确认当前网卡状态

```bash
cd track-af-xdp/lab-xdp-redirect-basics
./scripts/00_check_env.sh
```

如果输出里看不到 `ens192`，并且你之前跑过 DPDK，通常是 `0000:0b:00.0` 还绑定在 `uio_pci_generic`。恢复：

```bash
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
```

## 编译

```bash
./scripts/01_build_app.sh
```

## 跑 PASS 模式

```bash
sudo ./scripts/03_run_xdp_pass.sh
```

## 跑 DROP 模式

DROP 会影响目标网卡收包，所以必须确认，且脚本会拒绝对 `ens33` 操作：

```bash
sudo AF_XDP_CONFIRM_DROP=YES ./scripts/04_run_xdp_drop.sh
```

## 生成复盘

```bash
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```

## 下一站

这站通过后，进入：

```text
track-af-xdp/lab-af-xdp-socket-rings
```
