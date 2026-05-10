# SUMMARY

## Command sequence

```bash
./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
sudo ./scripts/03_run_copy_mode_baseline.sh
sudo ./scripts/04_probe_native_copy.sh
sudo ./scripts/05_probe_zero_copy.sh
./scripts/06_compare_modes.sh
./scripts/08_make_review_bundle.sh
```
