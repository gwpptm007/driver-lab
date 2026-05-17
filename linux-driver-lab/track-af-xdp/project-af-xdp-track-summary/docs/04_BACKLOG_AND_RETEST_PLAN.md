# Backlog and Retest Plan

## Backlog 1: lab-xdp-redirect-basics 补测

目标：从 `PASS_BASIC_ATTACH` 升级到 `PASS_ACTION / REDIRECT_MODEL_READY`。

建议执行：

```bash
cd track-af-xdp/lab-xdp-redirect-basics
./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_xdp_pass.sh
sudo AF_XDP_CONFIRM_DROP=YES ./scripts/04_run_xdp_drop.sh
sudo AF_XDP_CONFIRM_REDIRECT=YES ./scripts/05_run_xdp_redirect_dryrun.sh
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```

## Backlog 2: lab-af-xdp-socket-rings 测试

目标：证明 UMEM、AF_XDP socket、FILL/RX/TX/COMPLETION rings 可创建并运行。

```bash
cd track-af-xdp/lab-af-xdp-socket-rings
./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_af_xdp_socket_smoke.sh
./scripts/05_collect_stats.sh
./scripts/06_make_review_bundle.sh
```

## Backlog 3: zero-copy 探测

目标：明确 VMware/vmxnet3 环境是否支持 native / zero-copy。

```bash
cd track-af-xdp/lab-af-xdp-zero-copy-vs-copy
./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_copy_mode_baseline.sh
sudo ./scripts/04_probe_native_copy.sh
sudo ./scripts/05_probe_zero_copy.sh
./scripts/06_compare_modes.sh
./scripts/08_make_review_bundle.sh
```

## Backlog 4: mini forwarder 项目验证

目标：从 `READY_TO_TEST` 推进到 `PASS_DROP_SMOKE / PASS_REFLECT_SMOKE`，后续再补真实 traffic。

```bash
cd track-af-xdp/project-af-xdp-mini-forwarder
./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_forwarder_drop_smoke.sh
sudo ./scripts/04_run_forwarder_reflect_smoke.sh
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```
