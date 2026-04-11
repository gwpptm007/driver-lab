# Day33 本地运行手册

## 1. 首次准备

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day33
chmod +x scripts/*.sh
chmod +x guest/init.day33
source env/day33.env
source env/local_wq7.env 2>/dev/null || true
```

如无现成 `lspci`，先获取并构建：

```bash
make fetch-pciutils
make build-lspci
```

## 2. 默认全流程

```bash
make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
make backend
sudo -E make run
```

## 3. 跑完后优先看

```bash
cat records/day33-local-001/run-summary.md
cat records/day33-local-001/trace-config.txt
sed -n '1,160p' records/day33-local-001/trace-window.txt
cat records/day33-local-001/mmap-verify.txt
cat records/day33-local-001/run-result.txt
```

## 4. 当前包内自带 records 怎么理解

当前包已经自带一轮 `records/day33-local-001`，它对应的是**修复前现场**。阅读顺序建议是：

1. 先看 `mmap-verify.txt`，确认业务路径已通过
2. 再看 `trace-config.txt`，确认 tracefs 路径失败
3. 最后看 `run-summary.md`，确认这轮不能算通过

如果你要验证当前代码中的修复是否生效，请重新执行 `make run` 覆盖这轮 records。

## 5. 当前默认 workload

- `mmap-verify 64 0x55`
- 默认 trace workload：`verify`
- 如果想改成小规模 DMA 窗口：

```bash
export DAY33_TRACE_WORKLOAD=dma
export DAY33_TRACE_DMA_ITER=2
sudo -E make run
```

## 6. 解释 trace 的建议顺序

1. 先找到 `day33_ioctl`
2. 再看 `day33_do_run_dma`
3. 然后看两次 `day33_program_dma`
4. 最后看 `day33_wait_dma_idle` 与 `day33_irq_handler`

备注：day33 会自动探测 tracefs 根目录。若 guest 内只存在 `/sys/kernel/debug/tracing`，无需手工改脚本。
