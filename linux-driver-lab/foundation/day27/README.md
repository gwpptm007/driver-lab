# day27：remove/卸载对称性 + 200 次循环

## 1. 当天目标

Day27 不再追求新功能，而是验证 **已有驱动在长期重复装载/卸载下是否稳定**。

本日独立目录使用 QEMU EDU 设备，完成：
- 200 次 `insmod -> smoke -> rmmod` 循环
- 无 oops / panic / hung task
- `probe/remove` 次数与循环次数一致
- 每轮最小 smoke：触发一次 IRQ，确认 `irq_count > 0`

## 2. 主流程

先补执行位，再按本地环境文件运行：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day27
source env/local.wq7.env

chmod +x scripts/*.sh
chmod +x guest/init.day27
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true

make build-lspci
make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run
```

## 3. 结合当前测试结果的结论

本次 `records/day27-local-001/` 已证明 Day27 通过：
- EDU 设备成功枚举，`lspci-nn.txt` 中能看到 `1234:11e8`
- `loop-summary.txt` 显示 `loop_count=200 / pass=200 / fail=0`
- `serial.log` / `dmesg-driver.txt` 中反复出现 `probe success`、`irq handler`、`remove leave`
- `serial.log` 最后出现 `===DAY27:COMPLETE===`
- 未发现 `BUG:`、`Oops:`、`Kernel panic`、`hung task`

> 注意：当前上传记录中的 `run-summary.md` 里 `loop target met (200): no` 是脚本解析 `loop-summary.txt` 时未去掉回车导致的假阴性；
> 但同一批记录里的 `loop-summary.txt` 已明确给出 `pass=200 / fail=0`，串口日志也显示流程完整，因此最终结论仍然是 **通过**。

## 4. 必看文档

- `START_HERE.md`
- `docs/01_LOCAL_RUNBOOK.md`
- `docs/02_acceptance.md`
- `docs/03_TROUBLESHOOTING.md`
