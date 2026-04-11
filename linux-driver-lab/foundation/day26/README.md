# Day26 - 用户态工具与清晰错误码（最终验收版）

> 说明：day26 的模块必须通过顶层 `make module` 构建，不要进入 `day26/driver/` 后直接执行 `make`。
> 原因是顶层脚本会显式传入项目内 arm64 的 `KDIR`、`ARCH`、`CROSS_COMPILE`，并且 `driver/Makefile` 使用 `CURDIR` 来稳定输出 `day26_edu_tool.ko`。

## 目标

Day26 在 Day25 的 EDU + MSI 基础上，把字符设备接口做成更接近真实工具使用方式的闭环：

1. 保持 EDU 设备 `1234:11e8` 与 MSI 中断路径不变；
2. 驱动提供三类接口：
   - `ioctl`：结构化获取设备/中断状态、清零统计；
   - `read()`：直接返回可读状态文本；
   - `write()`：直接写入十进制/十六进制整数触发一次中断；
3. 用户态工具提供清晰子命令和清晰错误码；
4. guest 自动流程同时覆盖：
   - 正向路径：`info / read-state / count / status / trigger / reset-stats`；
   - 负向路径：`trigger 0`，验证错误码与错误信息可读；
5. 通过 `records/` 中的输出文件，证明：设备可见、probe 成功、工具可用、计数增长、错误输入提示清晰。

## 本轮测试结论

**Day26 通过。**

关键证据已经在 `records/day26-local-001/` 中：

- `run-summary.md`：摘要项全部为 `yes`；
- `lspci-nn.txt`：确认 EDU `1234:11e8` 已枚举；
- `dmesg-driver.txt`：确认 `probe success`、`MSI vector=50`、`irq handler` 已出现；
- `irq-count-before.txt` / `irq-count-after.txt`：驱动内部计数 `0 -> 1`；
- `proc-interrupts-before.txt` / `proc-interrupts-after.txt`：全局中断计数 `0 -> 1`；
- `invalid-trigger-zero.txt`：确认错误输入能返回 `Invalid argument` 与 `rc=5`；
- `serial.log`：出现 `===DAY26:COMPLETE===`，说明 guest 自动流程完整结束。

详细解释见：`docs/02_ACCEPTANCE.md`。

## 建议入口

- `START_HERE.md`
- `docs/01_LOCAL_RUNBOOK.md`
- `docs/02_ACCEPTANCE.md`
- `docs/03_TROUBLESHOOTING.md`
- `output/day26_quick_commands.md`
