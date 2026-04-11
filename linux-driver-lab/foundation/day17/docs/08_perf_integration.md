# 08_perf_integration - 在 Day17 中完整集成 perf

## 1. 这次 Day17 的 perf 集成目标

这次不是“手工拷一个 perf 进去能跑就算完”，而是把 perf 正式并入 Day17 自己的执行链：

1. `build_perf.sh` 能独立构建 arm64 perf
2. `build.sh` 能自动发现或自动构建 perf
3. `build.sh` 能把 perf 和它的动态依赖递归打进 rootfs
4. `guest_collect.sh` 能直接验证 perf 是否可执行
5. `host_collect.sh` 能自动把 perf 相关结果归档到 records/

---

## 2. 最推荐的完整命令

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17

export KERNEL_DIR=~/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=~/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-

PROFILE=baseline ./apply_config.sh
PERF_REQUIRED=yes PERF_MODE=auto ./build.sh
```

`PERF_MODE=auto` 的查找/构建顺序是：

1. `PERF_PATH`
2. `day17/output/perf/perf`
3. `KERNEL_SRC/tools/perf/perf`
4. 自动执行 `./build_perf.sh`

---

## 3. 单独构建 perf

如果你想先把 perf 单独编出来，再打进 rootfs，可以这样：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17
./build_perf.sh
```

成功后重点看：

```text
day17/output/perf/perf
day17/output/perf/perf.file.txt
day17/output/perf/perf.dynamic.txt
```

这三份文件的意义：

- `perf`：最终二进制
- `perf.file.txt`：检查是不是 aarch64 ELF
- `perf.dynamic.txt`：检查 NEEDED 依赖有哪些

---

## 4. build.sh 如何打包 perf 依赖

这版 `build.sh` 不只复制 `/usr/bin/perf`，还会：

1. 解析 ELF interpreter
2. 解析 NEEDED 依赖
3. 根据 `PERF_LIB_DIRS` / `PERF_SYSROOT` 找库
4. 递归复制依赖库自己的依赖
5. 生成 `day17/output/perf/perf_bundle_manifest.txt`
6. 把 manifest 放进 guest：`/etc/day17_perf_manifest.txt`

这样 guest 里如果 perf 报缺库，你就不用猜，而是直接看 manifest。

---

## 5. guest 里怎么验证

```sh
which perf
perf --version
perf list software | head
perf stat -e task-clock -- true
cat /etc/day17_perf_manifest.txt
```

Day17 当前把这几个动作也纳入 `guest_collect.sh` 了，所以自动采样完成后，在 `records/` 下也能看到：

- `perf_version.txt`
- `perf_list.txt`
- `perf_stat.txt`
- `perf_manifest.txt`

---

## 6. 如果你想用外部 perf

也可以显式传：

```bash
PERF_MODE=external \
PERF_PATH=/path/to/perf \
PERF_LIB_DIRS='/path/to/sysroot/lib:/path/to/sysroot/usr/lib:/path/to/sysroot/usr/lib/aarch64-linux-gnu' \
PERF_REQUIRED=yes \
./build.sh
```

这样 Day17 不会调用 `build_perf.sh`，只会使用你给出的 perf。

---

## 7. 最终验收标准

完整 perf 集成完成后，目标是：

```text
perf_bin_ok=yes
perf_smoke_ok=yes
```

并且 records 目录中 `perf_version.txt / perf_list.txt / perf_stat.txt` 都非空。


## 最终版的 perf smoke 约定

Day17 最终版已经把 `/bin/true` applet 链接默认打进 rootfs，
因此 guest_collect 的 perf smoke 统一使用：

```sh
perf stat -e task-clock -- /bin/true
```

这能避免最小 rootfs 中 workload 缺失导致的：

```text
Workload failed: No such file or directory
```
