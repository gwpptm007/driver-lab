# Day25 START_HERE

## 1. 进入 day25

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day25
source env/local.wq7.env
```

## 2. 准备 arm64 lspci（day25 独立目录完成）

```bash
mkdir -p third_party

git clone https://github.com/pciutils/pciutils.git third_party/pciutils
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true

make build-lspci
file third_party/pciutils/lspci
```

## 3. 构建并运行

```bash
make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run
```

## 4. 先看什么

```bash
cat records/${RUN_ID}/run-summary.md
sed -n '1,200p' records/${RUN_ID}/dmesg-driver.txt
cat records/${RUN_ID}/irq-count-before.txt
cat records/${RUN_ID}/irq-count-after.txt
sed -n '1,200p' records/${RUN_ID}/proc-interrupts-before.txt
sed -n '1,200p' records/${RUN_ID}/proc-interrupts-after.txt
```

## 5. 如何理解“通过”

优先看 `docs/02_RESULTS_AND_ACCEPTANCE.md`。这份文档已经结合上传的真实输出解释了：
- 为什么 `1234:11e8` 证明 EDU 枚举成功
- 为什么 `probe success + MSI vector` 证明驱动接管成功
- 为什么 `irq_count 0 -> 1` 证明 IRQ handler 真进入了
- 为什么 `/proc/interrupts` 中 `day25_edu_irq` 从 `0 -> 1` 增长证明内核全局中断统计也成立
- 为什么 `remove leave + ===DAY25:COMPLETE===` 说明 guest 自动流程完整结束
