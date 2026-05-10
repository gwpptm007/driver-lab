# lab-af-xdp-zero-copy-vs-copy exec board

| Step | Command | Expected |
|---|---|---|
| 1 | `./scripts/00_check_env.sh` | ENV_CHECK |
| 2 | `./scripts/01_build_app.sh` | BUILD_RESULT=PASS |
| 3 | `sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh` | vmxnet3 kernel driver |
| 4 | `sudo ./scripts/03_run_copy_mode_baseline.sh` | COPY_BASELINE.log |
| 5 | `sudo ./scripts/04_probe_native_copy.sh` | NATIVE_COPY_PROBE.log |
| 6 | `sudo ./scripts/05_probe_zero_copy.sh` | ZERO_COPY_PROBE.log |
| 7 | `./scripts/06_compare_modes.sh` | COMPARE_MODES.txt |
| 8 | `./scripts/08_make_review_bundle.sh` | REVIEW_BUNDLE.md |
