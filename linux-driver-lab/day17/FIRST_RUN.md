# Day17 FIRST_RUN - 第一次执行手册

## 1. 第一次建议顺序

现在 Day17 已经把 perf 集成也纳入了独立工作流，但第一次上手仍然建议分两段：

1. `apply_config.sh` —— 先把 Day17 的 profile 真正应用到内核配置中
2. `build.sh` —— 构造 rootfs / dtb，并手工进入 guest
3. guest 内执行 `/bin/day17_guest_collect.sh`
4. guest 手工确认 perf / tracing / demo 模块都正常
5. 最后再执行 `collect/host_collect.sh`

这样你第一次排错时，能把问题清楚分层：

- 内核配置问题
- rootfs 组装问题
- perf 构建/依赖问题
- 串口自动化问题

---

## 2. baseline + perf 推荐执行命令

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17

export KERNEL_DIR=~/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=~/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-

PROFILE=baseline ./apply_config.sh
PERF_REQUIRED=yes PERF_MODE=auto ./build.sh
```

如果你只想先跳过 perf，仍可：

```bash
PROFILE=baseline ./apply_config.sh
PERF_MODE=skip ./build.sh
```

---

## 3. guest 里最关键的手工验证

```sh
ls -l /bin/day17_guest_collect.sh
ls -l /demo_regmap.ko
which perf
perf --version
perf stat -e task-clock -- true
/bin/day17_guest_collect.sh
cat /tmp/day17-baseline/metrics.env
```

理想上应该看到：

```text
tracing_ok=yes
function_graph_ok=yes
perf_bin_ok=yes
perf_smoke_ok=yes
insmod_ok=yes
snapshot_ok=yes
trigger_ok=yes
```

如果 perf 不通，优先看：

- `/etc/day17_perf_manifest.txt`
- `/tmp/day17-baseline/perf_version.txt`
- `/tmp/day17-baseline/perf_stat.txt`

---

## 4. 回宿主机做自动采样

退出 QEMU 后执行：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17/collect
SCENARIO_ID=day17-baseline-arm64-virt ./host_collect.sh
```

如果你的 guest shell prompt 不是默认常见样式，也可以显式指定：

```bash
PROMPT='~ # ' SCENARIO_ID=day17-baseline-arm64-virt ./host_collect.sh
```

结果会在：

```text
day17/records/<timestamp>-day17-baseline-arm64-virt/
```

其中和 perf 最直接相关的文件是：

- `perf_version.txt`
- `perf_list.txt`
- `perf_stat.txt`
- `perf_manifest.txt`

---

## 5. build.sh 中 perf 的查找顺序

当你使用：

```bash
PERF_REQUIRED=yes PERF_MODE=auto ./build.sh
```

Day17 会按下面顺序找 perf：

1. `PERF_PATH` 指定的外部 perf
2. `day17/output/perf/perf`
3. `KERNEL_SRC/tools/perf/perf`
4. 自动调用 `./build_perf.sh` 现编

也就是说，这一版不再要求你每次都手工把 `PERF_PATH` 写死。

## 7. perf 最终版的一个关键变化

Day17 最终版已经把 `/bin/true` 固化进 rootfs，
因此你不再需要进 guest 手工执行：

```sh
ln -sf /bin/busybox /bin/true
```

现在 `/bin/day17_guest_collect.sh` 默认就会用：

```sh
perf stat -e task-clock -- /bin/true
```

来完成 perf smoke 测试。

如果你想看完整测试项，请直接阅读：

- `day17/docs/11_day17_full_test_checklist.md`


## 6. 跑 round1 / round2b 对比测试

当 baseline 已经稳定后，可以直接进入 Day17 的整轮对比：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17
export PERF_SYSROOT=/usr/aarch64-linux-gnu
export PERF_LIB_DIRS=/usr/aarch64-linux-gnu/lib

./run_compare_rounds.sh
```

如果你不想一次跑三轮，也可以拆成：

```bash
./run_profile_collect.sh round1
./run_profile_collect.sh round2b
python3 ./compare_results.py
```

## Round compare 证据链增强说明

当前版本在每轮 records 目录下都会额外保存 `build_evidence/`，并在批量对比后生成 `compare-*-*.diff`。  
如果你发现 baseline / round1 / round2b 的 boot_ms、image_kib 没差异，优先去看：

- `records/<...>/build_evidence/kernel.config`
- `records/<...>/build_evidence/artifact_evidence.env`
- `records/compare-*-baseline_vs_round1.diff`
- `records/compare-*-round1_vs_round2b.diff`

