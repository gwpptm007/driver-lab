# project-af-xdp-mini-forwarder exec board

| Step | Command | Expected | Status |
|---|---|---|---|
| env | `./scripts/00_check_env.sh` | ENV_CHECK | TODO |
| build | `./scripts/01_build_app.sh` | BUILD_RESULT=PASS | TODO |
| prepare | `sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh` | vmxnet3 + XDP detached | TODO |
| drop | `sudo ./scripts/03_run_forwarder_drop_smoke.sh` | FORWARDER_FINAL_STATS | TODO |
| reflect | `sudo ./scripts/04_run_forwarder_reflect_smoke.sh` | FORWARDER_FINAL_STATS | TODO |
| collect | `./scripts/06_collect_stats.sh` | parsed stats | TODO |
| bundle | `./scripts/07_make_review_bundle.sh` | REVIEW_BUNDLE.md | TODO |
