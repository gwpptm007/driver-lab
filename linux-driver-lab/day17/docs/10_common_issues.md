# 10_common_issues - 常见问题

## 1. build.sh 报 busybox 是动态链接版

这意味着当前 BusyBox 不能直接用于最小 initramfs。需要换成静态 BusyBox，或者先重新编译。

## 2. build_perf.sh 失败

优先检查：

- `KERNEL_SRC/tools/perf/Makefile` 是否存在
- `aarch64-linux-gnu-gcc` 是否存在
- 交叉环境的 sysroot / 目标开发库是否完整

Day17 的 `build_perf.sh` 已经尽量关闭了 Python / Perl / GTK / slang / libbpf 等无关特性，
但如果目标 sysroot 缺少 perf 当前仍需要的基础库，构建还是会失败。

## 3. guest 里执行 perf 报 not found，但文件明明存在

这通常不是 perf 本体没了，而是 loader 或依赖库没带全。优先检查：

- `/etc/day17_perf_manifest.txt`
- `records/.../perf_manifest.txt`
- `records/.../perf_version.txt`

如果 manifest 里有 `missing-interp` 或 `missing-needed`，说明 build.sh 当时没有在 `PERF_LIB_DIRS` 中找到对应库。

## 4. perf_bin_ok=yes，但 perf_smoke_ok=no

这说明 perf 已经能启动，但 `perf stat -e task-clock -- true` 没跑通。优先看：

- `records/.../perf_stat.txt`
- guest 中 `perf stat -e task-clock -- true` 的原始输出

常见原因：

- 当前内核/架构下 perf event 能力不完整
- 权限或配置项异常
- perf 本体能跑，但仍有运行期依赖问题

## 5. tracing_ok=no

优先检查：

- `apply_config.sh` 是否真的执行过 `olddefconfig`
- tracing 挂在 `/sys/kernel/debug/tracing` 还是 `/sys/kernel/tracing`
- `available_tracers.txt` 是否为空

## 6. host_collect.sh 等不到 prompt

优先检查：

- `serial.log`
- `rootfs.img` 与 `virt-day17.dtb` 是否来自当前 day17
- 如果你的 guest prompt 比较特殊，可以显式传：
  - `PROMPT='~ # '`
  - 或 `PROMPT_CANDIDATES='~ # |/ # |# '`

## 7. serial.log 里已经看到 `__DAY17_ENV_BEGIN__/END__`，但 `guest_metrics.env` 还是空

这通常不是 `guest_collect.sh` 没执行，而是 host 侧 marker 解析失败。当前新版脚本已经兼容 CRLF 和回显残留；如果仍复现，优先看：

```bash
grep -n '__DAY17_ENV_' serial.log
sed -n '230,265p' serial.log | sed -n 'l'
```


## 4. `which perf` 能看到 `/usr/bin/perf`，但执行仍然 `not found`

这通常不是 `PATH` 问题，而是 **AArch64 动态加载器/依赖库没有按目标机路径打进 rootfs**。

最常见的根因有两个：

- 把宿主机 `x86_64` 的 `libc.so.6` / `ld-linux-x86-64.so.2` 错打进 guest
- 交叉工具链的 `-print-sysroot` 退化成 `/`，脚本误去扫描宿主机 `/lib`、`/usr/lib`

Day17 当前脚本会优先用：

```bash
${CROSS_COMPILE}gcc -print-file-name=libc.so.6
${CROSS_COMPILE}gcc -print-file-name=libm.so.6
${CROSS_COMPILE}gcc -print-file-name=ld-linux-aarch64.so.1
```

来解析 **目标机库**，并拒绝把 `x86_64` 库打进 rootfs。

如果你想现场确认，最直接看这三项：

```bash
aarch64-linux-gnu-readelf -l output/perf/perf | grep 'Requesting program interpreter'
aarch64-linux-gnu-readelf -d output/perf/perf | grep NEEDED
sed -n '1,200p' output/perf/perf_bundle_manifest.txt
```


## perf --version 正常，但 perf_stat.txt 里仍然是旧失败结果

这通常不是新问题，而是你先手工把 `/bin/true` 补上并验证成功了，
但还没有重新运行一次：

```sh
rm -rf /tmp/day17-baseline
/bin/day17_guest_collect.sh
```

因为 `perf_stat.txt` 记录的是上一次 guest_collect 的结果；只要重新跑一遍，
就会刷新成新的成功输出。
