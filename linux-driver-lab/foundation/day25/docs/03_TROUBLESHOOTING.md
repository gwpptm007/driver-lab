# Day25 常见问题

## 1. `make module` 时 `modpost` 报 PCI 符号 undefined

说明内核 build tree 的 `Module.symvers` 还没按当前 arm64 PCI 配置重新生成。
先回内核源码目录跑：

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- O=... modules_prepare
make -j"$(nproc)" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- O=... modules
```

## 2. `third_party/pciutils/lib/configure: Permission denied`

zip 解压后，第三方源码里的可执行位可能丢失。执行：

```bash
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
```

## 3. `mknod ... /dev/console: Operation not permitted`

说明普通用户没有 `mknod` 权限。`rootfs` / `run` 这两步改用：

```bash
sudo -E make rootfs
sudo -E make run
```

## 4. `open /dev/day25_edu0 failed: No such device or address`

这通常不是 probe 失败，而是 guest 内字符设备节点不可用：
- 可能没挂 `devtmpfs`
- 可能设备节点被错误地 `mknod c 0 0`
- 也可能 sysfs 中 `major:minor` 没同步到 `/dev`

当前终版 `guest/init.day25` 已经修成：
- 挂 `devtmpfs`
- 若节点没自动出现，再从 `/sys/class/day25_edu/day25_edu0/dev` 读 major:minor 手工补建

## 5. 如何判断是“probe 成功但触发没闭环”

看这三组文件：
- `dmesg-driver.txt`：若有 `probe success` 和 `MSI vector=`，说明驱动准备已成功
- `irq-count-before/after.txt`：若 before/after 都是 0 或打不开设备，说明用户态触发链路没走通
- `proc-interrupts-before/after.txt`：若有 `day25_edu_irq` 条目但计数不变，则说明 IRQ 条目存在，但触发未成功

## 6. 当前上传 records 如何判断

当前上传这轮 `records/day25-local-001/` 已经不是“半通过”，而是完整闭环通过：
- `irq-count` 已经从 `0 -> 1`
- `/proc/interrupts` 中 `day25_edu_irq` 也从 `0 -> 1`
- `dmesg-driver.txt` 中有 `irq handler: irq=50 status=0x00000001 count=1`

所以如果你当前看到的是和这轮 records 相同的输出，就可以直接判定 day25 通过。
