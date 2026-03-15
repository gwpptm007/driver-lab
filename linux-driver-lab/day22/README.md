# day22：QEMU ivshmem PCI 设备可见性验证（重做版，补上 C 代码）

## 1. 这一天到底做什么

day22 的主目标仍然是：

1. 在 QEMU `virt` 平台上挂载一个 `ivshmem-doorbell` PCI 设备。
2. 让 arm64 guest 正常启动。
3. 在 guest 里跑出 `lspci -nn` 与 `lspci -vv -nn`。
4. 把 `dmesg`、`/sys/bus/pci/devices`、QEMU 启动命令这些证据归档到 `records/`。

但这次和上一版最大的区别是：

**day22 不再只有脚本和文档，而是补上了真正的 C 代码。**

也就是说，day22 现在同时包含两条线：

- **平台线**：把 PCI 设备稳定带进 guest，完成证据归档
- **代码线**：开始提供真正的 guest 侧 C 工具与 day23 可直接续写的 `pci_driver` stub

这样 day22 看起来就不再只是“搭环境”，而是已经进入“驱动作品的起跑线”。

---

## 2. 这次重做后，day22 里真正新增了什么 C 代码

### 2.1 guest 侧 C 工具：`tools/pci_sysfs_dump.c`

这是一个运行在 guest 里的最小 PCI 枚举工具。

它的设计目的不是替代 `lspci`，而是补一条更贴近驱动学习的观察路径：

- 不依赖 `libpci`
- 直接遍历 `/sys/bus/pci/devices`
- 打印 BDF、vendor/device/class、IRQ、resource
- 读取 config 空间前 64 字节做预览

为什么这个工具很有价值：

- 它让你看到：**即使还没写真正的 `pci_driver`，Linux 也已经通过 sysfs 暴露了 PCI 设备信息**
- day23 写 `probe()` 后，你可以把驱动日志和这个工具的输出对照着理解
- 它是最小 rootfs 里很适合保留的一类“观察型工具”

### 2.2 内核模块骨架：`driver/day22_ivshmem_stub.c`

这是 day22 提前放好的最小 `pci_driver` stub。

当前它只做这些事：

- 注册 `pci_device_id`
- 注册 `pci_driver`
- 提供 `probe/remove`
- 在 `probe()` 里打印 vendor/device/class/irq/BAR 资源信息
- 建立私有结构体 `struct day22_stub_dev`

它**故意不在 day22 就做完**：

- `pci_enable_device()`
- `pci_request_regions()`
- `pci_iomap()`
- `pci_alloc_irq_vectors()`

因为这些分别属于：

- day23：资源接管
- day25：MSI / IRQ

这样拆，阶段边界很清楚：

- day22：设备发现 + C 骨架
- day23：开始接管设备
- day25：开始处理中断

---

## 3. 当前 day22 的设计原则

这次按你的要求，**day22 相关代码和脚本全部放在 day22 内部实现**：

- 不调用前面 dayXX 的脚本
- 不复用前面 dayXX 的 rootfs 生成逻辑
- 不复用前面 dayXX 的 QEMU 启动逻辑
- 不复用前面 dayXX 的记录归档逻辑
- 不复用前面 dayXX 的驱动代码

唯一例外仍然是：

- **内核编译相关除外**
- 也就是 day22 会使用你本机已经准备好的：
  - `Image`
  - BusyBox
  - aarch64 交叉编译器
  - aarch64 `lspci`（或者本目录自己尝试构建）
  - QEMU / ivshmem-server
  - 内核构建目录 `KDIR`（仅在你想编译 stub 模块时需要）

---

## 4. 当前 day22 的真实交付内容

### 4.1 脚本与自动化

- `env/day22.env`
- `scripts/00_check_host_tools.sh`
- `scripts/01_check_kernel_config.sh`
- `scripts/02_build_guest_tools.sh`
- `scripts/02_build_guest_lspci.sh`
- `scripts/03_prepare_rootfs.sh`
- `scripts/04_start_ivshmem_server.sh`
- `scripts/05_run_qemu_ivshmem.sh`
- `scripts/06_extract_records.sh`
- `scripts/07_run_all.sh`
- `scripts/08_clean.sh`
- `scripts/09_build_stub_module.sh`

### 4.2 C 代码

- `tools/pci_sysfs_dump.c`
- `tools/Makefile`
- `driver/day22_ivshmem_stub.c`
- `driver/include/day22_ivshmem_stub.h`
- `driver/Makefile`

### 4.3 guest 入口与文档

- `guest/init.day22`
- `docs/`
- `output/`
- `records/`

---

## 5. day22 最小闭环

### 输入

- QEMU `virt` 平台
- `ivshmem-doorbell` 设备
- 你本机已经准备好的 arm64 `Image`
- 你本机已经准备好的 arm64 BusyBox
- 你本机已经准备好的 arm64 `lspci`（推荐静态链接）
- aarch64 交叉编译器（用于编译 `pci_sysfs_dump`）

### 过程

1. 检查宿主机工具与内核配置
2. 交叉编译 day22 自己的 guest C 工具 `pci_sysfs_dump`
3. 组装一个 **只服务于 day22** 的 initramfs
4. 启动 `ivshmem-server`
5. 启动 QEMU，并挂载 `ivshmem-doorbell`
6. guest 自动执行：
   - `lspci -nn`
   - `lspci -vv -nn`
   - `dmesg | grep -i pci`
   - `/bin/pci_sysfs_dump`
7. 自动关机
8. 主机侧把串口日志切分保存到 `records/<run-id>/`

### 输出

- `records/<run-id>/lspci-nn.txt`
- `records/<run-id>/lspci-vv-nn.txt`
- `records/<run-id>/dmesg-pci.txt`
- `records/<run-id>/sysfs-pci-devices.txt`
- `records/<run-id>/pci-config-dump.txt`
- `records/<run-id>/qemu-command.txt`
- `records/<run-id>/kernel-config-check.txt`
- `records/<run-id>/serial.log`
- `records/<run-id>/server.log`
- `records/<run-id>/run-summary.md`

---

## 6. 推荐执行顺序

### 第一步：看环境变量模板

```bash
cd linux-driver-lab/day22
sed -n '1,260p' env/day22.env
```

### 第二步：填写你本机路径

最少要准备：

- `KERNEL_IMAGE`
- `BUSYBOX_BIN`
- `GUEST_LSPCI_BIN`

如果你后面想在 day22 就把 stub 模块编出来，再额外准备：

- `KDIR`

示例：

```bash
export KERNEL_IMAGE=/path/to/arch/arm64/boot/Image
export KERNEL_CONFIG_PATH=/path/to/.config
export BUSYBOX_BIN=/path/to/busybox
export GUEST_LSPCI_BIN=/path/to/aarch64-static-lspci
export KDIR=/path/to/kernel/build
```

### 第三步：先检查，不直接启动

```bash
make check
```

### 第四步：先把 day22 自己的 guest C 工具编出来

```bash
make build-tools
```

### 第五步：构建 day22 独立 initramfs

```bash
make rootfs
```

### 第六步：整套跑一遍

```bash
make run
```

### 第七步：如果你想提前看 day23 的骨架，编一下 stub 模块

```bash
make module
```

---

## 7. 建议你今天重点看懂的两个文件

### 7.1 `tools/pci_sysfs_dump.c`

你要看懂的是：

- 为什么不依赖 `libpci`
- 为什么 day22 先走 sysfs
- `vendor/device/class/irq/resource/config` 分别代表什么

### 7.2 `driver/day22_ivshmem_stub.c`

你要看懂的是：

- `pci_device_id` 怎么匹配设备
- `pci_driver` 怎么注册
- `probe/remove` 生命周期如何组织
- 为什么 day22 只打印 BAR，不急着 `pci_iomap`

---

## 8. 当前没有假装完成的事情

下面这些事情，这次仍然没有假装已经跑通：

- 我没有在当前容器里真的启动你的 QEMU guest
- 我没有生成真实的 `records/<run-id>/lspci-vv-nn.txt`
- 我没有替你在当前容器里编译出 arm64 `lspci`
- 我也没有假装 `day22_ivshmem_stub.ko` 已经在你的内核里成功插入

这版交付是真正把 **代码、脚本、目录、注释、测试步骤** 都放进仓库，
让你在自己的真实环境里直接按 day22 自己的路径开工。

---

## 9. day22 通过标准

满足下面这些即可判定 day22 通过：

- guest 内 `lspci -nn` 能看到 `1af4:1110`
- `lspci -vv -nn` 能归档
- `pci_sysfs_dump` 输出中能看到目标设备 BDF 及 BAR/resource 信息
- `dmesg` 里能看到 PCI bus / host bridge / device 枚举相关信息
- `/sys/bus/pci/devices/` 中存在目标 BDF
- `records/<run-id>/` 内的证据文件齐全
- `driver/day22_ivshmem_stub.c` 可被阅读并作为 day23 的直接起点

---

## 10. 下一天怎么接

day22 完成后，day23 直接基于本目录里的 `driver/day22_ivshmem_stub.c` 进入：

- `pci_enable_device()`
- `pci_request_regions()`
- `pci_iomap()`
- BAR0 映射结果日志

也就是说：

**day22 解决“看见设备 + 先有 C 骨架”，day23 才开始“真正接管设备”。**
