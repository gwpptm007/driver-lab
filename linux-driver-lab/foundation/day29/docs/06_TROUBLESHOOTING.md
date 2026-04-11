# Day29 常见问题

## 1. `make build-lspci` 提示 `aarch64-linux-gnu-gcc` 不可执行

先确认交叉编译器本身是否在 PATH 中：

```bash
command -v aarch64-linux-gnu-gcc
command -v aarch64-linux-gnu-strip
```

如果这两个命令都能找到，而 day29 仍然报错，说明更可能是旧脚本把 `aarch64-linux-gnu-gcc` 当成了“当前目录下的文件名”在检查。
最新版 day29 已改为同时支持：

- `/usr/bin/qemu-system-aarch64` 这种带路径的可执行文件
- `aarch64-linux-gnu-gcc` 这种通过 PATH 查找的命令名

另外，如果 day27 已经编好可用的 arm64 静态 `lspci`，可以直接在 `env/local.<you>.env` 中把 `GUEST_LSPCI_BIN` 指向 day27 的产物，然后先跳过 `make build-lspci`。

## 2. `./configure: Permission denied`

zip 解压后执行位丢了：

```bash
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
```

## 3. `mknod ... Operation not permitted`

构建 rootfs 时创建 `/dev/console` 和 `/dev/null` 通常需要：

```bash
sudo -E make rootfs
```

## 4. `modpost: __pci_register_driver undefined`

先准备内核模块树：

```bash
make kernel-module-tree
```

脚本会先 `modules_prepare`，必要时再自动补一轮 `make modules`。

## 5. `/dev/day29_edu0` 没出现

guest 中会先依赖 `devtmpfs` 自动创建设备节点；
如果仍没出现，`guest/init.day29` 会从：

```bash
/sys/class/day29_edu/day29_edu0/dev
```

读取 `major:minor` 再手工 `mknod`。

## 6. `verify` 失败

优先看：

```bash
sed -n '1,200p' records/${RUN_ID}/dmesg-driver.txt
sed -n '1,120p' records/${RUN_ID}/verify-result.txt
```

优先排查：

- 长度是否超过 2048
- direction bit 是否写反
- DMA 地址是否误用了 CPU 虚拟地址
- `src/dst` 偏移是否重叠

## 7. `DMA mapping error`

Day29 走的是 `dma_alloc_coherent()`，如果这里仍报 DMA 相关错误，先检查：

- 驱动是否正确设置了 DMA mask
- guest / kernel 配置是否有明显异常
- 是否误把非 DMA 地址编程给设备

## 8. `tool-info.txt` 里 `dma_handle=0x0`

这通常意味着 coherent buffer 根本没申请成功，或者 probe 还没走到那一步。

优先看：

```bash
sed -n '1,200p' records/${RUN_ID}/dmesg-driver.txt
```

## 9. guest 串口没有 `===DAY29:COMPLETE===`

先看：

- `records/${RUN_ID}/serial.log`
- `records/${RUN_ID}/qemu.stderr.log`

常见原因：

- `insmod` 失败
- `/dev/day29_edu0` 没准备好
- 工具执行失败后脚本提前退出


## 本轮已知修复

- 修复 guest rootfs 中 busybox applet 链接缺失导致的 `/init: line 8: mount: not found` 与 `Attempted to kill init!`。
- `run_qemu` 已加入 `-no-reboot` 与宿主侧超时 (`QEMU_TIMEOUT_SEC`)，避免异常时 QEMU 长时间挂住不退出。


> arm64 virt 关键点：QEMU EDU 默认仅支持 28-bit DMA；在本实验中默认通过 `-device edu,dma_mask=0xffffffff` 放宽到 32-bit，并在驱动默认值中收口为 32-bit，guest 自动化不再依赖运行时模块参数传值。

## QEMU EDU 已枚举，但 `insmod ... dma_mask_bits=32` 报 `invalid parameter`

现象：

```
[    x.xxxxxx] day29_edu_dma: `' invalid for parameter `dma_mask_bits'
insmod: can't insert '/root/day29_edu_dma.ko': invalid parameter
```

结论：

- 这通常不是 `.ko` 不支持该参数；
- 更常见的是 guest 运行时的模块参数传递链路不稳定，最终内核收到的是空值 `dma_mask_bits=`；
- Day29 自动化已改为：驱动默认值直接收口为 32，guest init 不再依赖 `insmod ... dma_mask_bits=32`。

处理：

1. 更新到本次修复后的 day29；
2. 重新 `make module` 与 `sudo -E make rootfs`；
3. 再执行 `sudo -E make run`。
