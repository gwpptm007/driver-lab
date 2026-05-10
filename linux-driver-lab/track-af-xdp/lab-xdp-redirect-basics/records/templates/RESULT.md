# RESULT TEMPLATE

## Command sequence

```bash
./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_xdp_pass.sh
sudo AF_XDP_CONFIRM_DROP=YES ./scripts/04_run_xdp_drop.sh
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```

## Final decision

- [ ] PASS_BASIC
- [ ] PASS_ACTION
- [ ] REDIRECT_MODEL_READY

## Reviewer comment

