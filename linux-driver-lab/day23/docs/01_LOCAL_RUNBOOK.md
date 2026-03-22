# day23 本地执行手册（最终版）

## 1. 本轮前置背景

如果你是在 day22 之后直接推进到 day23，本轮有两个很重要的前置动作：

### 1) 重新打开 arm64 内核里的 PCI 相关配置
至少确认：

- `CONFIG_PCI=y`
- `CONFIG_PCI_MSI=y`
- `CONFIG_PCI_HOST_GENERIC=y`
- `CONFIG_MODULES=y`
- `CONFIG_MODULE_UNLOAD=y`

### 2) 重新生成内核模块树
仅仅重新编 `Image` 不够。day23 的外部模块要过 `modpost`，必须让当前 arm64 build tree 的 `Module.symvers` 跟新的 `.config` 同步。

推荐命令：

```bash
cd /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src

export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
unset CC CXX LD AR AS NM STRIP OBJCOPY OBJDUMP READELF

make -C /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src   ARCH=arm64   CROSS_COMPILE=aarch64-linux-gnu-   O=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64   modules_prepare

make -C /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src   -j"$(nproc)"   ARCH=arm64   CROSS_COMPILE=aarch64-linux-gnu-   O=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64   modules
```

如果这一步不做完整，外部模块很容易在 `modpost` 阶段报：

- `__pci_register_driver undefined`
- `pci_unregister_driver undefined`
- `pci_enable_device undefined`
- `pci_iomap undefined`

## 2. 进入 day23

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day23
```

## 3. 载入本地环境

```bash
source env/local.wq7.env
```

## 4. 正式执行顺序

```bash
make check
make kernel-module-tree
make module
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run
```

## 5. 关键中间检查

### 外部模块是否生成

```bash
ls -l driver/day23_ivshmem_probe.ko
```

### rootfs 是否生成

```bash
ls -l workdir/rootfs.img
```

### backend 是否生成

```bash
ls -l workdir/runs/${RUN_ID}/ivshmem-day23.bin
```

## 6. 跑完以后看哪里

```bash
cat records/${RUN_ID}/run-summary.md
sed -n '1,200p' records/${RUN_ID}/dmesg-probe.txt
sed -n '1,160p' records/${RUN_ID}/dmesg-remove.txt
grep -n 'DAY23\|probe\|remove\|BAR0\|BAR2\|ivshmem' records/${RUN_ID}/serial.log
cat records/${RUN_ID}/qemu.stderr.log
```

## 7. day23 通过标准

- `driver/day23_ivshmem_probe.ko` 已生成
- `run-summary.md` 中 6 项全是 `yes`
- `serial.log` 中出现：
  - `probe enter`
  - `BAR0:`
  - `BAR2:`
  - `probe success`
  - `remove enter`
  - `remove leave`
  - `===DAY23:COMPLETE===`
