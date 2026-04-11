# Day15 RESULTS - baseline 收口记录

## 1. Day15 已完成什么

Day15 已经把 W3 的 baseline 真正固定下来，形成了一套 **可重复执行、可自动汇总、可继续演进到 Day16** 的最小实验环境。

当前已经确认通过的能力：

- arm64 + QEMU virt 可启动到 shell
- Day15 独立目录可自举：`apply_config.sh` → `build.sh` → guest 采样 → host 汇总
- `demo_regmap.ko` 可加载
- debugfs 可用
- tracing / ftrace / function_graph 可用
- `snapshot` / `trigger` 可用
- baseline 数据可自动落盘到 `day15/records/<timestamp>-day15-baseline-arm64-virt/`

当前未纳入 Day15 硬验收、留给后续阶段的问题：

- `perf` 用户态程序尚未进入 rootfs（`perf_bin_ok=no`）
- 这项能力顺延到 **Day17 rootfs 工具补齐** 时再处理

---

## 2. 本次 baseline 的实际结果

### 2.1 基线场景

- 场景 ID：`day15-baseline-arm64-virt`
- 架构：`arm64`
- 机器：`QEMU virt`
- rootfs：`busybox-initramfs`
- 验证模块：`demo_regmap.ko`
- 内核版本：`5.15.10`

### 2.2 baseline.csv 中的关键结果

| 项目 | 数值 | 说明 |
|---|---:|---|
| Image 大小 | `39,799,296 bytes` | 启动镜像体积 |
| Image 大小 | `38,867 KiB` | 便于后续 D19 对比 |
| rootfs 大小 | `1,209,011 bytes` | BusyBox initramfs 体积 |
| rootfs 大小 | `1,181 KiB` | 便于后续 D17 / D19 对比 |
| boot_ms | `2008` | 从 QEMU 进程启动到 first shell prompt |
| MemTotal | `997,944 KiB` | 当前 guest 总内存视角 |
| MemFree | `968,564 KiB` | 当前 guest 空闲内存 |
| MemAvailable | `938,172 KiB` | 当前 guest 可用内存 |
| Slab | `12,252 KiB` | 内核 slab 开销 |
| SReclaimable | `6,800 KiB` | 可回收 slab |
| SUnreclaim | `5,452 KiB` | 不可回收 slab |
| KernelStack | `744 KiB` | 内核栈占用 |
| PageTables | `104 KiB` | 页表开销 |
| modules_loaded_count_before | `0` | guest 采样前尚未加载 demo 模块 |
| modules_loaded_count_after | `1` | guest 采样后已加载 `demo_regmap.ko` |
| tracing_ok | `yes` | tracing 目录已可用 |
| function_graph_ok | `yes` | `function_graph` tracer 已可用 |
| trace_smoke_ok | `yes` | tracer 切换冒烟成功 |
| insmod_ok | `yes` | demo 模块可加载 |
| snapshot_ok | `yes` | debugfs snapshot 可读 |
| trigger_ok | `yes` | trigger 触发可写 |
| perf_bin_ok | `no` | rootfs 暂未内置 `perf` 程序 |

### 2.3 available_tracers 结果

```text
function_graph wakeup_dl wakeup_rt wakeup irqsoff function nop
```

---

## 3. 测试过程：每一步在哪里执行、执行什么、看什么、记录什么

### Step 1：应用 Day15 tracing 基线配置并重编内核

**执行位置：宿主机**

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day15
export KERNEL_DIR=~/workspace/driver-lab/kernel-src/linux-5.15.10
export CROSS_COMPILE=aarch64-linux-gnu-
./apply_config.sh
```

**看什么：**

- `olddefconfig` 成功结束
- 内核成功完成 `make`
- 日志中能看到 `CONFIG_TRACING` / `CONFIG_FTRACE` / `CONFIG_FUNCTION_GRAPH_TRACER` 等项已生效
- `output/arm64/Image` 最终存在

**记录什么：**

- 这一步成功与否
- 是否确实把 `Image` 同步到 `kernel-src/linux-5.15.10/output/arm64/Image`
- 如果失败，记录失败阶段：`olddefconfig` / `make` / `Image sync`

### Step 2：构造 Day15 实验环境并启动 QEMU

**执行位置：宿主机**

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day15
./build.sh
```

**看什么：**

- `demo_regmap.ko` 编译成功
- `rootfs.img` 生成成功
- `virt-day15.dtb` 生成成功
- QEMU 最终启动成功，并出现 shell prompt

**记录什么：**

- `rootfs.img` 是否生成
- `virt-day15.dtb` 是否生成
- `build.sh` 是否能把 QEMU 拉起
- 如果失败，记录失败位置：模块编译 / rootfs 打包 / DT 注入 / QEMU 启动

### Step 3：在 guest 内手工执行采样脚本

**执行位置：guest shell**

```sh
/bin/day15_guest_collect.sh
cat /tmp/day15-baseline/metrics.env
cat /tmp/day15-baseline/available_tracers.txt
```

**看什么：**

- `metrics.env` 中至少应看到：
  - `boot_ok=yes`
  - `debugfs_ok=yes`
  - `tracing_ok=yes`
  - `function_graph_ok=yes`
  - `trace_smoke_ok=yes`
  - `insmod_ok=yes`
  - `snapshot_ok=yes`
  - `trigger_ok=yes`
- `available_tracers.txt` 中应看到 `function_graph`

### Step 4：退出当前 guest，切回宿主机跑自动汇总

**执行位置：guest shell**

```sh
poweroff -f
```

### Step 5：宿主机自动采集并生成 baseline.csv

**执行位置：宿主机**

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day15/collect
PROMPT='~ # ' ./host_collect.sh
```

**看什么：**

- 记录目录下成功生成：
  - `baseline.csv`
  - `metrics.env`
  - `serial.log`

---

## 4. 结果文件在哪里看

- 自动采集总目录：`linux-driver-lab/day15/records/`
- 最关键文件：`baseline.csv`、`metrics.env`、`serial.log`

---

## 5. Day15 过程中踩过的关键问题（已修正）

- 内核目录不是传统单层结构，而是 `src / build / output`
- tracing 本身不是脚本问题，而是内核配置未打开
- `host_collect.sh` 抽取串口 block 时需要处理 ``

---

## 6. Day15 可以怎样正式收口

- [x] baseline 场景已确定（arm64 + QEMU virt + BusyBox initramfs + demo_regmap）
- [x] tracing/function_graph 所需内核配置已补齐并验证生效
- [x] Day15 独立实验目录已建立
- [x] rootfs / dtb / demo 模块可独立构造
- [x] guest 手工采样可成功
- [x] host 自动汇总可成功生成 `baseline.csv`
- [x] baseline 关键结果已记录到本文件
- [ ] perf 用户态程序进入 rootfs（留到 Day17）
