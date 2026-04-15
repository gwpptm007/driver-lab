# scripts

建议职责：

- `build.sh`：编译 stage07 驱动
- `run.sh`：加载模块、准备测试环境
- `smoke.sh`：跑最小 smoke
- `stats_check.sh`：核对 queue / irq / napi 统计
- `trace_smoke.sh`：收集行为级 trace 样本

当前先提供脚本骨架，后续实现时再补齐细节。
