# day22 排障说明

## 1. `make check` 就失败

### 现象

- 找不到 `qemu-system-aarch64`
- 找不到 `ivshmem-server`
- 找不到 `Image`
- 找不到 BusyBox

### 处理

先把主机工具与路径修正好，再继续。

---

## 2. `lspci` 在 guest 里跑不起来

### 常见原因

- 你拷进去的是宿主机 x86_64 `lspci`
- arm64 `lspci` 是动态链接，但 rootfs 里没有对应 libc / libpci

### 建议

优先使用 **arm64 静态 `lspci`**。

---

## 3. guest 启动了，但 `lspci` 看不到 ivshmem

### 先检查

- `qemu-command.txt` 里是否真的带了 `-device ivshmem-doorbell`
- `ivshmem-server` 是否已成功创建 socket
- 内核是否开启了 PCI / PCI host / MSI

### 证据文件

- `qemu-command.txt`
- `server.log`
- `kernel-config-check.txt`
- `serial.log`

---

## 4. QEMU 超时退出

### 常见原因

- guest 卡在早期启动
- `init` 没生成对
- BusyBox 不是 arm64
- `poweroff` 没执行到

### 先看

- `serial.log`
- `qemu.stderr.log`

---

## 5. `records/` 里切分文件为空

### 常见原因

- guest init 没走到对应 marker
- 串口日志格式被改坏
- `lspci` 命令提前失败

### 处理

先直接打开 `serial.log`，搜索：

- `===DAY22:LSPCI_NN:BEGIN===`
- `===DAY22:LSPCI_VV_NN:BEGIN===`
- `===DAY22:COMPLETE===?`

如果 marker 都没有，先修 guest 启动链路，不要急着修抽取脚本。
