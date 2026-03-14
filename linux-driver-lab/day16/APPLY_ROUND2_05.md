# Day16 APPLY_ROUND2_05

## 1. round2 / round2b 的目标

在 round1 的基础上，继续清理：

- DRM / 显示平台残余链
- SoundWire
- I2C helper
- USB ChipIdea

并在必要时升级为 round2b：直接收显示栈上层。

---

## 2. round2 准备阶段（已执行并记录）

### 执行位置

全部在 **宿主机** 执行：

```bash
cd ~/workspace/driver-lab/kernel-src/linux-5.15.10
```

### round2 已尝试关闭的项

- 大部分 DRM 平台驱动
- `SOUNDWIRE`
- `I2C_GPIO`
- `USB_CHIPIDEA`

### round2 准备阶段结论

- `USB_CHIPIDEA_TEGRA` 不再出现，视为依赖坍塌正常
- `DRM_DW_HDMI` 仍为 `=m`
- `I2C_ALGOBIT` 仍为 `=m`
- tracing / perf / debug 主链仍然完好

因此 round2 升级为 **round2b**。

---

## 3. round2b 的思路

不再追叶子项：

- `DRM_DW_HDMI`
- `I2C_ALGOBIT`

而是直接去追它们的上游来源：

- `CONFIG_DRM`
- `CONFIG_DRM_KMS_HELPER`
- `CONFIG_DRM_FBDEV_EMULATION`
- `CONFIG_DRM_BRIDGE`
- `CONFIG_DRM_SUN4I`

---

## 4. round2b 当前执行步骤

### 执行位置

**宿主机**：

```bash
cd ~/workspace/driver-lab/kernel-src/linux-5.15.10
```

### 当前已经做到哪里

- 已备份 `.config.day16_round2_prep.bak`
- 已把 round2b 项写入 `.config`
- 已执行 `olddefconfig`
- 已确认：
  - `# CONFIG_DRM is not set`
  - tracing / perf / debug 主链仍然存在

### 当前下一步（还未验证完成）

1. 重新编译内核
2. 同步新的 `Image`
3. 回到 `day15/` 执行 `./build.sh`
4. 进入 guest 后执行：

```sh
/bin/day15_guest_collect.sh
cat /tmp/day15-baseline/metrics.env
cat /tmp/day15-baseline/available_tracers.txt
```

5. guest 验证通过后，再在宿主机执行：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day15/collect
PROMPT='~ # ' SCENARIO_ID='day16-round2b-arm64-virt' ./host_collect.sh
```

---

## 5. round2b 编译前主链检查（已通过）

仍然保留：

- `CONFIG_DEBUG_FS=y`
- `CONFIG_TRACEPOINTS=y`
- `CONFIG_TRACING=y`
- `CONFIG_FTRACE=y`
- `CONFIG_FUNCTION_TRACER=y`
- `CONFIG_FUNCTION_GRAPH_TRACER=y`
- `CONFIG_DYNAMIC_FTRACE=y`
- `CONFIG_KALLSYMS=y`
- `CONFIG_KALLSYMS_ALL=y`
- `CONFIG_PERF_EVENTS=y`
- `CONFIG_HW_PERF_EVENTS=y`
