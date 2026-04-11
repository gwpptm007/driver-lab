# Day15 - baseline 选择与现状采样（自包含版本）

## 1. 这次为什么要把 Day15 和 Day13 分离

你这次提的方向是对的：

> **Day15 是 W3 的 baseline 实验，不应该再默认挂靠到 day13 上。**

前一版虽然能跑，但有一个结构性问题：

- Day15 的 rootfs、DTB、启动路径默认复用 day13
- 一旦 Day15 结果异常，不容易判断是：
  - Day15 采样脚本的问题
  - 还是 day13 构建/脚本带来的问题

所以这一版重新收口后，Day15 明确变成一个**自包含实验目录**：

```text
day15/
├── README.md
├── Makefile
├── build.sh
├── demo_regmap.c
├── demo_regmap.fragment.dtsi
├── inject_virt_dt.py
├── function_graph_targets.txt
├── baseline_template.csv
├── collect/
│   ├── guest_collect.sh
│   ├── host_collect.sh
│   └── parse_meminfo.awk
└── records/
```

也就是说：

- `demo_regmap.ko` 由 `day15/Makefile` 自己编译
- `rootfs.img` 由 `day15/build.sh` 自己生成
- `virt-day15.dtb` 由 `day15/build.sh` 自己生成
- baseline 采样由 `day15/collect/*.sh` 自己完成

之后你做 D16 / D17 / D18，也建议优先在 `day15/` 这条链路上继续迭代，而不是再回到 day13 里改。

---

## 2. Day15 当前目标

Day15 不做裁剪，不改功能，只做一件事：

> **先把 W3 后续所有对比要用到的 baseline 场景和采样口径固定下来。**

所以 Day15 的关键词是：

- baseline
- 冻结
- 采样
- 记录
- 可复现

这一天最重要的不是“把系统做小”，而是先回答：

- 当前用哪套场景做基线？
- 采哪些字段？
- 怎么采才可比？
- 结果落在哪里？

---

## 3. Day15 统一基线场景

这一版 Day15 固定到下面这个场景：

- 架构：`arm64`
- 虚拟平台：`QEMU virt`
- 内核产物：`Image`
- rootfs：`BusyBox + initramfs(rootfs.img)`
- 验证模块：`demo_regmap.ko`
- 调试能力：`debugfs + tracing/function_graph（记录现状）`
- perf：先记录“有没有”，D17 再补用户态二进制

为什么验证模块还是 `demo_regmap.ko`？

因为 Day15 的目标不是再发明新驱动，而是需要一个已经稳定、便于采样的验证对象。它已经具备：

- `/sys/kernel/debug/demo_regmap/snapshot`
- `/sys/kernel/debug/demo_regmap/trigger`

非常适合做 baseline 的最小冒烟动作。

---

## 4. Day15 现在有哪些脚本，它们分别做什么

### 4.1 `build.sh`

运行位置：**宿主机，`day15/` 目录下**

职责：

1. 编译 `demo_regmap.ko`
2. 构造 Day15 自己的最小 rootfs
3. 把 `/bin/day15_guest_collect.sh` 打进 rootfs
4. 导出并注入 QEMU virt DTB
5. 生成 `virt-day15.dtb`
6. 直接启动 QEMU

它的目标是：

> 让你第一次先手工进入 guest，把基本链路看清楚。

### 4.2 `collect/guest_collect.sh`

运行位置：**guest 内**

职责：

- 挂载 `proc/sys/dev/debugfs/tracefs`
- 保存 `meminfo/modules/mount/filesystems/dmesg`
- 自动探测 tracing 路径：
  - `/sys/kernel/debug/tracing`
  - `/sys/kernel/tracing`
- 检查 `function_graph`
- 尝试 `insmod /demo_regmap.ko`
- 读取 `snapshot`
- 执行一次最小 `trigger`
- 生成 `/tmp/day15-baseline/metrics.env`

### 4.3 `collect/host_collect.sh`

运行位置：**宿主机，`day15/collect/` 目录下**

职责：

- 统计 `Image/rootfs.img/.ko` 的大小或数量
- 启动 QEMU 并记录 `serial.log`
- 计算 `boot_ms`
- 通过串口自动调用 `/bin/day15_guest_collect.sh`
- 从 `serial.log` 提取 guest 输出
- 生成 `records/.../baseline.csv`

这三个脚本的关系不是“谁替代谁”，而是：

- `build.sh`：先把环境搭起来，让你手工验证
- `guest_collect.sh`：验证 guest 侧采样动作
- `host_collect.sh`：把整条 baseline 变成自动化采集

---

## 5. Day15 推荐执行顺序（第一次一定按这个来）

第一次最稳的方式，不是直接跑 `host_collect.sh`。

推荐顺序：

### 第一步：先构建并手工进入 guest

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day15

export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-

./build.sh
```

如果你的路径不是上面这个，就改成你本地真实路径。

`./build.sh` 跑完后会直接进入 QEMU。

---

### 第二步：在 guest 里手工验证 Day15 基本链路

QEMU 起来后，先执行：

```sh
ls -l /bin/day15_guest_collect.sh
ls -l /demo_regmap.ko
```

确认这两个文件都在。

然后执行：

```sh
/bin/day15_guest_collect.sh
cat /tmp/day15-baseline/metrics.env
```

这是第一次最关键的一步。

你至少应该看到类似：

```text
boot_ok=yes
debugfs_ok=yes
tracing_ok=yes 或 no
function_graph_ok=yes 或 no
insmod_ok=yes
snapshot_ok=yes
trigger_ok=yes
```

如果 `tracing_ok/function_graph_ok` 还是 `no`，这一版也会额外保存：

- `mount.txt`
- `filesystems.txt`
- `available_tracers.txt`

这样就比前一版更容易定位到底是：

- tracing 挂在了 `/sys/kernel/tracing`
- 还是内核根本没开到位

---

### 第三步：退出 QEMU，回到宿主机做自动采样

因为 `host_collect.sh` 会自己再启动一次 QEMU，
所以你在 guest 里手工验证完成后，先退出：

```sh
poweroff -f
```

回到宿主机后再执行：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day15/collect

export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
export KDIR=$KERNEL_DIR/build/arm64
export KERNEL_IMG=$KERNEL_DIR/output/arm64/Image
export QEMU_BIN=qemu-system-aarch64

./host_collect.sh
```

注意：

这一版的 `host_collect.sh` 默认已经指向：

- `day15/rootfs.img`
- `day15/virt-day15.dtb`

所以它不再要求你手工写 `DAY13_DIR`。

---

### 第四步：查看采样结果

执行成功后，结果会在：

```text
day15/records/YYYYMMDD-HHMMSS-day15-baseline-arm64-virt/
```

重点看：

```bash
cat baseline.csv
cat metrics.env
less serial.log
```

如果自动化失败，优先看 `serial.log`。

---

## 6. Day15 第一次最小验收标准

第一次先不要追求所有项目都完美，先看下面这些是否成立。

### 6.1 手工 guest 验收

在 `/tmp/day15-baseline/metrics.env` 中至少应满足：

- `boot_ok=yes`
- `debugfs_ok=yes`
- `insmod_ok=yes`
- `snapshot_ok=yes`
- `trigger_ok=yes`

如果再满足：

- `tracing_ok=yes`
- `function_graph_ok=yes`

那 Day15 baseline 的可观测性就更完整。

### 6.2 自动采样验收

`day15/records/.../` 目录下至少应有：

- `serial.log`
- `metrics.env`
- `baseline.csv`
- `meminfo.txt`
- `modules.txt`
- `available_tracers.txt`

---

## 7. baseline_template.csv 字段说明

当前 CSV 头如下：

```csv
scenario_id,kernel_ver,arch,machine,rootfs_type,demo_module,collector_ver,image_bytes,image_kib,rootfs_bytes,rootfs_kib,modules_built_count,boot_start_event,boot_end_event,boot_ms,boot_note,memtotal_kib,memfree_kib,memavailable_kib,slab_kib,sreclaimable_kib,sunreclaim_kib,kernelstack_kib,pagetables_kib,modules_loaded_count,debugfs_ok,tracing_ok,function_graph_ok,trace_smoke_ok,perf_bin_ok,perf_smoke_ok,boot_ok,insmod_ok,snapshot_ok,trigger_ok,dmesg_warn,remarks
```

字段可以分成几组理解：

### 7.1 元信息

- `scenario_id`
- `kernel_ver`
- `arch`
- `machine`
- `rootfs_type`
- `demo_module`
- `collector_ver`

### 7.2 构建产物信息

- `image_bytes`
- `image_kib`
- `rootfs_bytes`
- `rootfs_kib`
- `modules_built_count`

### 7.3 启动时间

- `boot_start_event`
- `boot_end_event`
- `boot_ms`
- `boot_note`

### 7.4 guest 内存

- `memtotal_kib`
- `memfree_kib`
- `memavailable_kib`
- `slab_kib`
- `sreclaimable_kib`
- `sunreclaim_kib`
- `kernelstack_kib`
- `pagetables_kib`

### 7.5 可观测性/功能状态

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
- `remarks`

---

## 8. tracing 路径为什么要专门处理

你前面已经遇到过一个很典型的问题：

```text
debugfs_ok=yes
tracing_ok=no
function_graph_ok=no
```

这种情况不一定表示“内核没有 tracing”，很可能只是：

- 脚本只检查了 `/sys/kernel/debug/tracing`
- 但你的系统实际把 tracing 挂在 `/sys/kernel/tracing`

所以这一版 `guest_collect.sh` 会：

1. 先尝试挂载 `debugfs`
2. 再尝试挂载 `tracefs`
3. 自动优先检查：
   - `/sys/kernel/debug/tracing`
   - `/sys/kernel/tracing`

这样更适合后面做裁剪实验时的 baseline 记录。

---

## 9. 当前版本的边界

这一版已经把 Day15 从 day13 中拆开了，但也要明确它现在还**没有**做什么：

### 9.1 还没有把 perf 正式打进 rootfs

所以 `perf_bin_ok` 目前大概率仍然会是：

- `no`
- `pending`

这是正常的 baseline 现状，不是失败。

### 9.2 还没有做完整 function_graph trace 归档

Day15 现在只是记录：

- tracing 有没有
- `function_graph` 在不在
- tracer 能不能切换

真正完整的 trace 文本归档、截图留档，更适合继续放在 D16/D17 之后做，或者另开 Day16+ 的脚本。

### 9.3 还没有做 D20 回归框架

`host_collect.sh` 是 D20 自动化的雏形，但当前重点还是：

> 先把 baseline 采集链跑顺。

---

## 10. 你现在应该怎么用这版 Day15

最推荐的节奏是：

### 10.1 先只跑到 guest 手工采样

```bash
cd day15
./build.sh
```

进入 guest 后执行：

```sh
/bin/day15_guest_collect.sh
cat /tmp/day15-baseline/metrics.env
```

先把这一步跑顺。

### 10.2 然后再做宿主机自动采样

```bash
cd day15/collect
./host_collect.sh
```

### 10.3 再开始下一步迭代

等 Day15 baseline 稳定后，再继续：

- D16：第一轮粗裁
- D17：rootfs 方案与工具补足
- D18：分类裁剪
- D19：对比报告
- D20：自动回归
- D21：最终 1-2 页报告

---

## 11. 当前版本建议

这一版最大的改动不是多了几个脚本，而是**边界终于清楚了**：

- Day13：偏“已有驱动 + ftrace 路径观察”
- Day15：偏“baseline 场景冻结 + 数据采样”

两个主题可以共享同一个教学驱动思路，
但**不应该再共享默认构建入口和默认 rootfs 产物**。

这也是后面继续做 W3 时，更稳的组织方式。
