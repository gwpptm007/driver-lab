# Day17 START_HERE

如果你刚打开 day17/，建议按下面顺序看：

1. `README.md`
2. `FIRST_RUN.md`
3. `docs/03_profiles_and_apply_config.md`
4. `docs/04_build_rootfs_and_qemu.md`
5. `docs/08_perf_integration.md`
6. `docs/09_result_reading.md`

如果你现在只想直接跑完整 baseline + perf，最短路径是：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17
PROFILE=baseline ./apply_config.sh
PERF_REQUIRED=yes PERF_MODE=auto ./build.sh
```

进入 guest 后：

```sh
which perf
perf --version
perf stat -e task-clock -- true
/bin/day17_guest_collect.sh
cat /tmp/day17-baseline/metrics.env
```


## 推荐新增阅读

- `docs/17_day17_final_summary_and_round_compare.md`
- `docs/18_day17_final_test_process.md`
- `docs/19_day17_implementation_walkthrough.md`
- `docs/20_day17_flowcharts_and_uml.md`
