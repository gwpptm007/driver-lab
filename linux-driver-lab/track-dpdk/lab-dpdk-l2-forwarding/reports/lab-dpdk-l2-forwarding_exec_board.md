# lab-dpdk-l2-forwarding_exec_board

| Step | Command | Expected | Status |
|---|---|---|---|
| 1 | `./scripts/00_check_env.sh` | 生成 ENV_CHECK，确认 libdpdk/meson/ninja/devbind | TODO |
| 2 | `./scripts/01_build_app.sh` | 生成 `app/build/l2fwd-lite` | TODO |
| 3 | `sudo ./scripts/02_prepare_vmxnet3.sh` | hugepage OK，0000:0b:00.0 绑定 uio_pci_generic | TODO |
| 4 | `sudo ./scripts/03_run_l2fwd_single_port.sh` | port started，进入 forwarding loop，stats 输出 | TODO |
| 5 | `./scripts/06_collect_stats.sh` | 收集 hugepage/devbind/log grep/dmesg | TODO |
| 6 | `./scripts/07_make_review_bundle.sh` | 生成 REVIEW_BUNDLE.md | TODO |

## PASS_SMOKE 标准

```text
BUILD.log: 编译成功
L2FWD_SINGLE_PORT.log: port 0 started
L2FWD_SINGLE_PORT.log: enter forwarding loop
L2FWD_SINGLE_PORT.log: rte_eth_stats
L2FWD_SINGLE_PORT.log: bye
```

## PASS_FORWARDING 标准

```text
两个或更多 port 初始化成功
rx/tx/ipackets/opackets 非 0
tx_failed 不持续增长
```
