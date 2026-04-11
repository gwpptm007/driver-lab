# 03_profiles_and_apply_config - profile 与内核配置

## 1. Day17 的三个 profile

### baseline
只应用：

- `config/trace_baseline.fragment`

目标：

- 保证 debugfs / tracefs / ftrace / function_graph / perf event 等基础能力可用
- 作为所有后续对比的基线

### round1
应用：

- `trace_baseline.fragment`
- `trim_round1.fragment`

目标：

- 先去掉一批对当前 arm64 + QEMU virt baseline 明显无关的驱动块

### round2b
应用：

- `trace_baseline.fragment`
- `trim_round1.fragment`
- `trim_round2b.fragment`

目标：

- 在 round1 基础上继续裁掉残余的部分显示 / i2c helper / usb 平台项

## 2. 命令示例

```bash
PROFILE=baseline ./apply_config.sh
PROFILE=round1   ./apply_config.sh
PROFILE=round2b  ./apply_config.sh
```

如果你只想改 `.config` 但暂时不编内核：

```bash
BUILD_KERNEL=no PROFILE=round2b ./apply_config.sh
```
