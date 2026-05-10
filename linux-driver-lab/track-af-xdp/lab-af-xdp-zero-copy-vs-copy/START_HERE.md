# START_HERE

本实验的重点是“模式边界”，不是追求一定 zero-copy 成功。

先跑：

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

输出重点看：

```text
records/*-af-xdp-zero-copy-vs-copy/COPY_BASELINE.log
records/*-af-xdp-zero-copy-vs-copy/NATIVE_COPY_PROBE.log
records/*-af-xdp-zero-copy-vs-copy/ZERO_COPY_PROBE.log
records/*-af-xdp-zero-copy-vs-copy/COMPARE_MODES.txt
records/*-af-xdp-zero-copy-vs-copy/REVIEW_BUNDLE.md
```
