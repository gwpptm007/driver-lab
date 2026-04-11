# Day25 本地运行手册

## 0. 目标

Day25 要验证：
- EDU 设备可见 (`1234:11e8`)
- `pci_alloc_irq_vectors()` 成功申请 MSI vector
- 驱动 `probe()` 成功
- 用户态工具能打开 `/dev/day25_edu0`
- 触发 EDU 中断后，驱动 `irq_count` 增长
- `/proc/interrupts` 对应 IRQ 计数增长

## 1. 进入 day25

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day25
source env/local.wq7.env
```

## 2. 准备 arm64 lspci（Day25 独立目录内完成）

```bash
mkdir -p third_party

git clone https://github.com/pciutils/pciutils.git third_party/pciutils
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true

make build-lspci
file third_party/pciutils/lspci
```

期望看到：
- `ARM aarch64`
- 最好 `statically linked`

## 3. 准备模块构建树

```bash
make check
make kernel-module-tree
```

## 4. 构建 day25 代码

```bash
make build-tools
make module
```

## 5. 构建 rootfs 并运行

```bash
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run
```

## 6. 看结果

```bash
cat records/${RUN_ID}/run-summary.md
sed -n '1,160p' records/${RUN_ID}/irq-info-before.txt
cat records/${RUN_ID}/irq-count-before.txt
cat records/${RUN_ID}/irq-count-after.txt
sed -n '1,200p' records/${RUN_ID}/proc-interrupts-before.txt
sed -n '1,200p' records/${RUN_ID}/proc-interrupts-after.txt
grep -n 'DAY25\|probe\|irq handler\|remove' records/${RUN_ID}/serial.log
```

## 7. 当前上传 records 的解释

这次上传的 `records/day25-local-001/` 已经证明 day25 全闭环成功：

- `lspci-nn.txt` 中出现 `1234:11e8`，说明 EDU 设备成功枚举
- `dmesg-driver.txt` 中有 `probe success` 和 `MSI vector=50`，说明驱动成功申请到 MSI
- `irq-info-before.txt` 中能成功读取 `irq_vector=50 irq_count=0 msi_enabled=1`，说明用户态工具已经成功打开字符设备
- `trigger.txt` 中有 `triggered value=0x00000001`，同时 `dmesg-driver.txt` 中出现 `irq handler: irq=50 status=0x00000001 count=1`
- `irq-count-before.txt` / `irq-count-after.txt` 显示 `0 -> 1`
- `proc-interrupts-before.txt` / `proc-interrupts-after.txt` 中 `day25_edu_irq` 一行显示 `0 -> 1`
- `serial.log` 中有 `remove enter`、`remove leave` 和 `===DAY25:COMPLETE===`

因此，这一轮 records 已经构成完整的“EDU + MSI”实验闭环证据。
