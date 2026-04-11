# Day26 本地运行手册

## 目标

验证下面 5 条链路都成立：

1. EDU 设备可见；
2. 驱动 probe / remove 成功；
3. 用户态工具 `ioctl / read / write` 都可用；
4. 中断触发成功，驱动内部计数与 `/proc/interrupts` 计数都增长；
5. 错误输入 `trigger 0` 返回清晰错误信息。

## 步骤

### 1. 进入 day26 并加载本地环境

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day26
source env/local.wq7.env
```

### 2. 准备 arm64 `lspci`（只在 day26 当前目录内）

```bash
mkdir -p third_party
# git clone https://github.com/pciutils/pciutils.git third_party/pciutils

# zip 解压后 configure 可能丢执行位，建议显式补一次
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true

make build-lspci
file third_party/pciutils/lspci
```

期望看到 `ARM aarch64`，最好 `statically linked`。

### 3. 补脚本与 guest init 执行位

```bash
chmod +x scripts/*.sh
chmod +x guest/init.day26
```

说明：zip 解压后脚本的 `+x` 可能会丢；虽然新版脚本已经尽量用 `bash scripts/xxx.sh` 调用，但把执行位补齐更稳，也更方便你手工单步调试。

### 4. 构建与运行

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

### 5. 查看结果

```bash
cat records/${RUN_ID}/run-summary.md
cat records/${RUN_ID}/info-before.txt
cat records/${RUN_ID}/read-state-before.txt
cat records/${RUN_ID}/irq-count-before.txt
cat records/${RUN_ID}/irq-count-after.txt
cat records/${RUN_ID}/proc-interrupts-before.txt
cat records/${RUN_ID}/proc-interrupts-after.txt
cat records/${RUN_ID}/invalid-trigger-zero.txt
grep -n 'DAY26\|probe\|trigger\|irq handler\|remove' records/${RUN_ID}/serial.log
```

### 6. 当前这轮测试如何快速判通过

看下面 6 条：

1. `run-summary.md` 里所有关键项是 `yes`；
2. `info-before.txt` 里 `vendor=0x1234 device=0x11e8` 且 `msi_enabled=1`；
3. `irq-count-before.txt` 为 `0`，`irq-count-after.txt` 为 `1`；
4. `proc-interrupts-before.txt` 为 `0`，`proc-interrupts-after.txt` 为 `1`；
5. `invalid-trigger-zero.txt` 里出现 `Invalid argument` 与 `rc=5`；
6. `serial.log` 里有 `===DAY26:COMPLETE===`。
