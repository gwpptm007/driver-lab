# Day17 完整测试清单（最终版）

## 1. 测试目标

确认 Day17 已形成独立闭环，并完成以下验证：

1. `arm64 + QEMU virt` 可正常启动
2. BusyBox initramfs 可正常工作
3. `demo_regmap.ko` 可正常加载并触发
4. `debugfs / tracing / function_graph` 可用
5. `guest_collect.sh` 能输出完整采样结果
6. `host_collect.sh` 能自动回收 guest 输出并生成 `metrics.env / baseline.csv`
7. `perf` 已成功集成进 rootfs，并能执行基础 `perf stat`

---

## 2. 构建前准备

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17

export CROSS_COMPILE=aarch64-linux-gnu-
export PERF_SYSROOT=/usr/aarch64-linux-gnu
export PERF_LIB_DIRS=/usr/aarch64-linux-gnu/lib
```

先确认交叉工具链和 QEMU 可用：

```bash
aarch64-linux-gnu-gcc --version
aarch64-linux-gnu-readelf --version
qemu-system-aarch64 --version
```

---

## 3. baseline 配置

```bash
PROFILE=baseline ./apply_config.sh
```

### 验证点

- 脚本执行成功
- tracing/function_graph/perf 相关配置已应用

---

## 4. 构建 Day17 + perf

```bash
PERF_REQUIRED=yes PERF_MODE=auto ./build.sh
```

### 验证点

- 内核与模块构建成功
- rootfs 打包成功
- perf 安装到 rootfs 成功

### 宿主机检查

```bash
sed -n '1,200p' output/perf/perf_bundle_manifest.txt
find rootfs -name 'ld-linux-aarch64.so*' -o -name 'libc.so*' -o -name 'libm.so*'
```

预期至少能看到：

```text
rootfs/lib/ld-linux-aarch64.so.1
rootfs/lib/libc.so.6
rootfs/lib/libm.so.6
```

同时 rootfs 最终版还应默认包含：

```text
rootfs/bin/true
```

---

## 5. guest 手工测试

进入 guest 后，先看欢迎信息和 prompt：

```text
Linux Driver Lab Day17 guest ready (self-contained)
~ #
```

### 5.1 模块加载

```sh
insmod /demo_regmap.ko
```

### 5.2 运行 guest 采样

```sh
/bin/day17_guest_collect.sh
cat /tmp/day17-baseline/metrics.env
```

### 5.3 tracing / function_graph

```sh
cat /sys/kernel/debug/tracing/available_tracers
```

预期包含：

```text
function_graph
```

### 5.4 snapshot

```sh
cat /tmp/day17-baseline/snapshot.txt
```

预期能看到 `module=demo_regmap`、寄存器状态、IRQ/work 统计等。

---

## 6. perf 集成验证

### 6.1 perf 路径

```sh
which perf
```

预期：

```text
/usr/bin/perf
```

### 6.2 perf 版本

```sh
perf --version
```

预期类似：

```text
perf version 5.15.10
```

### 6.3 true applet

最终版 rootfs 已默认提供 `/bin/true`，因此无需再手工：

```sh
ln -sf /bin/busybox /bin/true
```

你仍可检查：

```sh
ls -l /bin/true
busybox --list | grep '^true$'
```

### 6.4 perf smoke

```sh
perf stat -e task-clock -- /bin/true
```

预期会输出 `task-clock` 统计。

### 6.5 检查 guest_collect 导出的 perf 文件

```sh
cat /tmp/day17-baseline/perf_version.txt
cat /tmp/day17-baseline/perf_stat.txt
```

预期：

- `perf_version.txt` 非空
- `perf_stat.txt` 包含 `task-clock`

### 6.6 guest metrics 关键字段

```sh
cat /tmp/day17-baseline/metrics.env
```

最终希望看到：

```env
perf_bin_ok=yes
perf_smoke_ok=yes
```

---

## 7. host 自动采样

宿主机执行：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day17/collect
SCENARIO_ID=day17-baseline-arm64-virt ./host_collect.sh
```

### 7.1 检查 records 目录

预期生成：

```text
day17/records/<timestamp>-day17-baseline-arm64-virt/
```

### 7.2 检查关键文件

目录中至少应有：

```text
host_metrics.env
guest_metrics.env
metrics.env
baseline.csv
serial.log
meminfo.txt
modules.txt
snapshot.txt
tracing_dir.txt
perf_version.txt
perf_stat.txt
```

### 7.3 检查 marker

```bash
grep -n '__DAY17_HOST_HANDSHAKE__' serial.log
grep -n '__DAY17_GUEST_CMD_RC__' serial.log
grep -n '__DAY17_ENV_' serial.log
```

预期能看到：

- `__DAY17_HOST_HANDSHAKE__`
- `__DAY17_GUEST_CMD_RC__0`
- `__DAY17_ENV_BEGIN__`
- `__DAY17_ENV_END__`

### 7.4 最终 records/metrics.env

```bash
cat metrics.env
```

预期核心字段：

```env
boot_ok=yes
debugfs_ok=yes
tracing_ok=yes
function_graph_ok=yes
trace_smoke_ok=yes
perf_bin_ok=yes
perf_smoke_ok=yes
insmod_ok=yes
snapshot_ok=yes
trigger_ok=yes
dmesg_warn=no
```

### 7.5 最终 baseline.csv

```bash
cat baseline.csv
```

应正确输出：

- `scenario_id`
- `boot_ms`
- `image_kib`
- `rootfs_kib`
- `modules_built_count`
- `modules_loaded_count`
- `debugfs_ok`
- `tracing_ok`
- `function_graph_ok`
- `trace_smoke_ok`
- `perf_bin_ok`
- `perf_smoke_ok`
- `boot_ok`
- `insmod_ok`
- `snapshot_ok`
- `trigger_ok`
- `dmesg_warn`

---

## 8. 当前最终结论

Day17 最终版可以定义为：

> **Day17 已完成独立 baseline 收口，并完成 perf 集成与自动化 smoke 测试。**

也就是说：

- baseline 独立链通过
- host / guest 自动采样通过
- perf 集成通过
- perf smoke 自动化通过
