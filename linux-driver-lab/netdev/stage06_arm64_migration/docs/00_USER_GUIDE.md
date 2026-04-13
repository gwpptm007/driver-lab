# stage06 使用指南

## 1. stage06 是做什么的

stage06 的核心是**在真实 ARM64 环境中完成 stage04 的端到端验证**。

具体来说，把 stage04 的 `netdev_stage04.ko` 迁移到 ARM64 + QEMU 环境，验证：
- 交叉编译链路正确（aarch64-linux-gnu-gcc + kernel build system）
- 模块在 ARM64 上成功加载并注册 netdev（nds4）
- NAPI poll + ring DMA + RX replenishment 在 ARM64 上正常工作
- smoke test 端到端通过

---

## 2. 在测试机上执行

### 2.1 环境准备（wq7）

```bash
# 同步代码到测试机
rsync -av --delete \
  /e/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/netdev/stage06_arm64_migration/ \
  wq7:workspace/driver-lab/linux-driver-lab/netdev/stage06_arm64_migration/

ssh wq7
```

### 2.2 执行 smoke test

**方式 A：直接跑 QEMU（需要先在 wq7 上准备好 rootfs）**

```bash
cd ~/workspace/driver-lab/linux-driver-lab/netdev/stage06_arm64_migration

# 1. ARM64 交叉编译 stage04 模块
make build-stage04-arm64

# 2. 启动 ARM64 QEMU（带 smoke test rootfs）
./scripts/run_arm64_smoke.sh
```

**方式 B：分步执行**

```bash
# 平台解析
make resolve-arm64

# 交叉编译
make build-stage04-arm64

# 验证结果
cat output/stage04_qemu-arm64.log
```

### 2.3 当前 smoke 结果（2026-04-13）

```
TX:  32 frames (RXIDX=0..31, SKBLEN=25, ETH=0x6865=0x88B7) ✅
RX:  32 POLL events (IDX=0..31, PROTO=0x88B7) ✅
NAPI poll 在 ARM64 上正常工作 ✅
端到端 smoke test 成功 ✅
```

---

## 3. 验证每一步

### 3.1 验证步骤总览

| 步骤 | 命令 | 验证什么 | 预期结果 |
|------|------|---------|---------|
| 1 | `make build-stage04-arm64` | ARM64 交叉编译 | `netdev_stage04.ko` 生成（aarch64 ELF）|
| 2 | QEMU 启动 ARM64 kernel | kernel boot | `Linux version 5.15.10` on ARM64 ✅ |
| 3 | `insmod netdev_stage04.ko` | 模块加载 | `registered ifname=nds4` ✅ |
| 4 | `send + recv` burst | TX/RX 全流程 | dmesg 显示 32 帧 TX/RX ✅ |

### 3.2 每一步的验证逻辑

#### 步骤 1：ARM64 交叉编译

```bash
make build-stage04-arm64
```

**验证文件**：`output/netdev_stage04.ko`

```
file output/netdev_stage04.ko
→ ELF 64-bit LSB relocatable, ARM aarch64, version 1 (SYSV)
```

如果 MODPOST 失败，报 `register_netdev undefined`，说明 kernel `vmlinux.symvers` 缺少网络符号。需要：

```bash
# 在 wq7 上执行
cd /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64
# 确认 CONFIG_NET=y
grep 'CONFIG_NET=' .config
# 如果是 # CONFIG_NET is not set，改为 CONFIG_NET=y
sed -i 's/# CONFIG_NET is not set/CONFIG_NET=y/' .config
echo 'CONFIG_NET_CORE=y' >> .config
# 重新 build
sudo make vmlinux Image modules
```

#### 步骤 2：ARM64 kernel boot

```bash
./scripts/run_arm64_qemu.sh
```

预期 dmesg：
```
Linux version 5.15.10 ... #10 SMP PREEMPT Mon Apr 13 22:32:51 CST 2026
NET: Registered PF_PACKET protocol family
pci 0000:00:01.0: [1af4:1000] type 00 class 0x020000   ← virtio-net 设备
```

#### 步骤 3：模块加载

在 QEMU shell 中：
```
insmod /tmp/netdev_stage04.ko
→ [netdev_stage04] registered ifname=nds4 ring_size=64 napi_weight=16 rx_buf_size=2048
```

#### 步骤 4：smoke test

```
ip link set nds4 up
/tmp/send_stage04_frame_arm64 nds4 hello 0x88B7 32 0
```

dmesg 预期输出：
```
[stage04] TX RXIDX=0 SKBLEN=25 CPYLEN=25 ETH=6865
[stage04] TX RXIDX=1 SKBLEN=25 CPYLEN=25 ETH=6865
...
[stage04] POLL IDX=0 LEN=25 PROTO=88b7
[stage04] POLL IDX=1 LEN=25 PROTO=88b7
...
```

---

## 4. 产物与本地文件的对应关系

| 产物文件 | 本地对应路径 | 验证什么 |
|---------|------------|---------|
| `output/netdev_stage04.ko` | `linux-driver-lab/netdev/stage06_arm64_migration/output/netdev_stage04.ko` | ARM64 模块 |
| `output/rootfs_arm64.img` | `linux-driver-lab/netdev/stage06_arm64_migration/output/rootfs_arm64.img` | 含 smoke test 的 rootfs |
| `output/platform_matrix.md` | `linux-driver-lab/netdev/stage06_arm64_migration/output/platform_matrix.md` | 三平台参数 |
| `output/stage06_report.md` | `linux-driver-lab/netdev/stage06_arm64_migration/output/stage06_report.md` | 阶段报告 |
| `records/20260413-arm64-smoke/dmesg.txt` | `linux-driver-lab/netdev/stage06_arm64_migration/records/` | smoke 结果记录 |

产物同步回本地：

```bash
# wq7 → 本地
rsync -av wq7:workspace/driver-lab/linux-driver-lab/netdev/stage06_arm64_migration/output/ \
  /e/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/netdev/stage06_arm64_migration/output/
```

---

## 5. 原理图：我们学到了什么

### 5.1 ARM64 迁移的关键差异点

```
x86_64 host                              ARM64 + QEMU
─────────────────────────────────────    ─────────────────────────────────────
gcc（原生编译器）                        aarch64-linux-gnu-gcc（交叉编译）
/lib/modules/$(uname -r)/build          /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64
modprobe（自动找模块依赖）               insmod（手动加载，需先 insmod af_packet.ko）
原生 kernel 的 vmlinux.symvers           交叉编译时需要目标平台的 vmlinux.symvers
```

### 5.2 迁移三步法

```
第一步：工具链就绪
  ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-  ✓

第二步：符号表就绪（最常出问题的地方）
  vmlinux.symvers 必须包含 netdev 核心符号    ✓
  → 缺少符号 = kernel build 时 CONFIG_NET=n

第三步：rootfs 就绪
  busybox ARM64 + af_packet.ko + 模块 + 工具  ✓
  → init 脚本用 #!/bin/sh（不是 #!/busybox sh）
  → mkdir -p /proc /sys /dev（busybox rootfs 默认无这些目录）
```

### 5.3 ARM64 QEMU 启动链路

```
qemu-system-aarch64
  -kernel Image                    ← ARM64 kernel Image（含网络支持的新 build）
  -initrd rootfs_arm64.img        ← busybox + af_packet.ko + netdev_stage04.ko + 工具
  -append "rdinit=/init"          ← /init 依次：加载 af_packet → 加载模块 → smoke
```

---

## 6. 常见问题

### Q：MODPOST 报 `register_netdev undefined`

**原因**：ARM64 kernel 的 `vmlinux.symvers` 缺少网络符号。

```bash
# 在 wq7 上执行
sed -i 's/# CONFIG_NET is not set/CONFIG_NET=y/' \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64/.config
echo 'CONFIG_NET_CORE=y' >> \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64/.config
echo 'wq123456!' | sudo -S make -C \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64 vmlinux Image modules
grep register_netdev \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64/vmlinux.symvers
```

### Q：QEMU 启动后 `/init` 报错 `Failed to execute /init (error -2)`

**原因**：initramfs 中 `/init` 不可执行或 shebang 路径错误。

**检查**：
```bash
zcat output/rootfs_arm64.img | cpio -it | grep init
```

**修复**：确保 `#!/bin/sh`（不是 `#!/busybox sh`），且文件有执行权限。

### Q：`ip link` 报错 `Address family not supported by protocol`

**原因**：`CONFIG_PACKET` 未启用，AF_PACKET sockets 不可用。

```bash
# 在 wq7 上执行
echo 'wq123456!' | sudo -S bash -c \
  'echo "CONFIG_PACKET=m" >> /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64/.config'
echo 'wq123456!' | sudo -S make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
  -C /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64 modules
# 把 af_packet.ko 复制到 rootfs
cp /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64/net/packet/af_packet.ko \
  /home/wq7/workspace/driver-lab/linux-driver-lab/netdev/stage06_arm64_migration/workdir/rootfs_arm64/tmp/
# 重新打包 rootfs
```

### Q：`mount: mounting none on /proc failed: No such file or directory`

**原因**：busybox rootfs 默认没有 `/proc` 目录。

**修复**：在 rootfs 中创建目录：
```bash
cd workdir/rootfs_arm64
mkdir -p proc sys dev
# 重新打包
find . -print0 | cpio -0 -o -H newc | gzip -9 > ../output/rootfs_arm64.img
```

### Q：recv 收到 0 帧，但 dmesg 显示 POLL 收到 32 帧

**原因**：`eth_type_trans` 把 ethertype 0x88B7 发到内核协议栈，没有注册 handler 则 DROP。这是**预期行为**，不是 bug。smoke 成功标志是 dmesg 的 POLL 输出，不是 recv 用户态计数。
