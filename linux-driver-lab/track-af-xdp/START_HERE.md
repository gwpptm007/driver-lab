# track-af-xdp START_HERE

当前建议直接进入：

```bash
cd track-af-xdp/project-af-xdp-mini-forwarder
```

先跑：

```bash
./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_forwarder_drop_smoke.sh
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```
