# stage06 深度指南 — ARM64 迁移与跨平台兼容

## 一、stage06 在整个学习路径中的位置

stage06 是 W5 的第六天，承接 stage05 的平台参数化，引入**跨平台构建链路**和**真实 ARM64 环境验证**。

```
W5: DMA + performance (day29-35)
├── day29: DMA 基础 (dma_alloc_coherent / dma_map_page)
├── day30: mmap 零拷贝
├── day31: benchmarking 吞吐量 / 延迟
├── day32: perf / ftrace 性能分析
├── day33: stage04 ring + DMA + RX replenishment
├── day34: stage05 virtio-net 源码阅读 + 平台参数化
├── day35: stage06 ARM64 迁移               ← 今天
└── day36+: stage07 继续
```

**stage06 的本质**：不写新功能代码，而是做一次**平台迁移验证**——把 stage04 的驱动成果推广到 ARM64 + QEMU 环境，确认构建/运行/观测链路全通。

---

## 二、三层平台架构

### 2.1 三条路径总览

```
host（x86_64）                    qemu-x86_64                      qemu-arm64
───────────────────────────────── ───────────────────────────────── ─────────────────────────────────────
native gcc                         gcc                               aarch64-linux-gnu-gcc
/lib/modules/$(uname -r)/build    kernel build tree                 kernel build tree (ARM64)
native kernel                      kernel Image (bzImage)            kernel Image (Image)
modprobe                           QEMU + initramfs                  QEMU + initramfs (ARM64)
native rootfs                      busybox rootfs (x86_64)           busybox rootfs (ARM64)
```

### 2.2 三种编译模式

| 模式 | ARCH | CROSS_COMPILE | KDIR | 编译器 |
|------|------|--------------|------|--------|
| host | (空) | (空) | /lib/modules/$(uname -r)/build | gcc |
| qemu-x86_64 | (空) | (空) | kernel build tree | gcc |
| qemu-arm64 | arm64 | aarch64-linux-gnu- | ARM64 kernel build tree | aarch64-linux-gnu-gcc |

---

## 三、ARM64 交叉编译原理

### 3.1 交叉编译的本质

交叉编译是在 x86_64 host 上编译出 ARM64 目标架构的代码。流程：

```
x86_64 host                              ARM64 target
──────────────────────────               ─────────────
aarch64-linux-gnu-gcc  ← 交叉编译器       生成 ARM64 ELF
+ kernel build (arm64)  ← 目标内核头文件
                           + ARM64 内核模块 Makefile
                           ↓
                    netdev_stage04.ko (ARM64 ELF)
                    ↓
              QEMU ARM64 加载运行
```

### 3.2 外部模块编译的正确方式

```bash
make -C KDIR M=PWD modules
#   ↑        ↑     ↑    ↑
#   |        |     |    当前目录（模块源码）
#   |        |     Module.symvers 输出目录
#   |        kernel build tree
#   kernel build system entry point
```

**关键**：`ARCH=$(TARGET_ARCH) CROSS_COMPILE=$(CROSS_COMPILE)` 必须通过 make 传递，不能只设置环境变量：

```bash
# 正确
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C /path/to/arm64/kdir M=$(PWD) modules

# 错误：环境变量在 make 内部不传递
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
make -C /path/to/kdir M=$(PWD) modules  # ARCH/CROSS_COMPILE 传不进去
```

原因：make 的 `-C` 切换目录后，子 make 的环境变量继承需要显式传递。

---

## 四、vmlinux.symvers 与 MODPOST

### 4.1 内核符号表体系

```
vmlinux.symvers      — kernel built-in 符号（kernel 编译时生成）
Module.symvers        — 模块自己的导出符号（模块编译时生成）
Module.markers        — 符号标记
```

外部模块编译时，MODPOST 需要从 `vmlinux.symvers` 解析 kernel built-in 符号，从 `Module.symvers` 获取模块自身符号。

### 4.2 缺符号的本质

当 `vmlinux.symvers` 中缺少 `register_netdev` 等符号时，MODPOST 报错：

```
ERROR: "register_netdev" [netdev_stage04.ko] undefined!
```

**原因**：kernel .config 中 `CONFIG_NET=n`，net/core 模块没有被编译，其符号未导出到 vmlinux.symvers。

### 4.3 修复路径

```
1. 修改 .config：CONFIG_NET=y, CONFIG_NET_CORE=y
2. touch net/core/dev.c（让 build system 认为需要重新编译）
3. make vmlinux（重新链接，触发 net/core 编译 + 符号导出）
4. make Image（生成新 kernel image）
5. make modules（生成完整 Module.symvers）
```

### 4.4 vmlinux.symvers 验证

```bash
# 在 ARM64 kernel build 目录执行
grep register_netdev vmlinux.symvers
# 应输出：0x12345678 register_netdev net/core/dev.o

# 对比大小
wc -l vmlinux.symvers
# 正常：~11000 行（充足的网络符号）
# 异常：~8000 行（缺少网络符号）
```

---

## 五、ARM64 QEMU 虚拟平台

### 5.1 QEMU ARM64 启动链路

```
qemu-system-aarch64
  -machine virt                     ← virt 机型（PCI 总线 + virtio-mmio）
  -cpu cortex-a57                   ← ARMv8-A CPU
  -kernel Image                     ← ARM64 kernel Image
  -initrd rootfs_arm64.img         ← initramfs（busybox + ko + tools）
  -append "rdinit=/init console=ttyS0" ← kernel cmdline
```

### 5.2 virt 机型设备表

| 设备 | QEMU 参数 | 说明 |
|------|-----------|------|
| 串口 | -serial mon:stdio | ttyS0 控制台 |
| virtio-net | 自动（PCI） | 设备 ID 1af4:1000 |
| RAM | -m 1G | 1GB 内存 |
| GIC | 内置 | ARM 中断控制器 |

### 5.3 PCI 设备枚举（ARM64 virt）

ARM64 上 virtio-net 作为 PCI 设备出现：

```
[    0.000000] pci 0000:00:01.0: [1af4:1000] type 00 class 0x020000
[    0.000000] pci 0000:00:01.0: reg 0x10: [io  0x0000-0x003f]
[    0.000000] pci 0000:00:01.0: reg 0x14: [mem 0x00000000-0x0000003f]
```

这与 stage04 的模拟设备（`1af4:1000`）在 PCI 层面匹配。

---

## 六、rootfs 构建与 init 流程

### 6.1 busybox rootfs 目录结构

```
workdir/rootfs_arm64/
├── bin/
│   ├── busybox          ← static linked ARM64 busybox
│   └── sh -> busybox    ← /bin/sh 指向 busybox
├── tmp/
│   ├── netdev_stage04.ko
│   ├── af_packet.ko
│   ├── send_stage04_frame_arm64
│   └── recv_stage04_frame_arm64
├── proc/                 ← mkdir -p proc sys dev 后打包
├── sys/
├── dev/
└── init                  ← entry point
```

### 6.2 init 脚本流程

```bash
#!/bin/sh
mkdir -p /proc /sys /dev
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev  # 或 mount -t tmpfs none /dev

echo "[stage06] ARM64 rootfs up"

insmod /tmp/af_packet.ko
insmod /tmp/netdev_stage04.ko
ip link set nds4 up

# 后台接收
/tmp/recv_stage04_frame_arm64 nds4 0x88B7 32 5 &

# 前台发送
sleep 1
/tmp/send_stage04_frame_arm64 nds4 hello_arm64 0x88B7 32 0

sleep 3
cat /sys/kernel/debug/netdev_stage04/stats 2>/dev/null
cat /sys/kernel/debug/netdev_stage04/rings 2>/dev/null

exec /bin/sh
```

### 6.3 三个常见 rootfs 问题

**问题 1：shebang 错误**

错误：`#!/busybox sh` → 尝试执行 `/busybox`（不存在）

正确：`#!/bin/sh` → busybox 的 `/bin/sh` 是指向 `busybox` 的符号链接

**问题 2：缺少目录**

busybox rootfs 默认没有 `/proc`、`/sys`、`/dev`，导致 `mount -t proc none /proc` 失败。

修复：`mkdir -p proc sys dev` 在打包前

**问题 3：mdev /dev  flooding**

busybox 的 mdev 可能导致 stderr 大量 `can't open /dev/tty*` 错误。

修复：使用 `mount -t devtmpfs none /dev` 或 `mount -t tmpfs none /dev`

---

## 七、兼容层设计（netdev_kcompat.h）

### 7.1 兼容层的目标

跨 kernel 版本和跨平台时，某些 API 行为不同。兼容层提供统一抽象：

```c
// include/netdev_kcompat.h

// u64_stats 差异：
// - 5.15-6.7: void u64_stats_update_begin(struct u64_stats_sync *syncp)
// - 6.8+:   bool u64_stats_update_begin(struct u64_stats_sync *syncp)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0)
#define NETDEV_KCOMPAT_U64_STATS_UPDATE_BEGIN(syncp) \
    u64_stats_update_begin(syncp)
#define NETDEV_KCOMPAT_U64_STATS_UPDATE_END(syncp) \
    u64_stats_update_end(syncp)
#else
#define NETDEV_KCOMPAT_U64_STATS_UPDATE_BEGIN(syncp) \
    do { u64_stats_update_begin(syncp); } while (0)
#define NETDEV_KCOMPAT_U64_STATS_UPDATE_END(syncp) \
    u64_stats_update_end(syncp)
#endif
```

### 7.2 兼容层内容

| 兼容项 | 问题 | 解决方案 |
|--------|------|----------|
| `u64_stats_update_begin` 返回值 | 6.8+ 返回 bool，之前 void | 版本判断 + 宏抽象 |
| NAPI 权重 | 旧版 `weight` vs 新版 `budget` | 统一 `napi_weight` 参数 |
| `eth_type_trans` | 4.10+ 参数变化 | 版本判断 |
| `netif_napi_add` | 参数顺序变化 | 统一调用封装 |

---

## 八、ARM64 smoke test 全流程

### 8.1 执行顺序

```
1. resolve-arm64      → 解析 ARM64 平台参数
2. build-stage04-arm64 → 交叉编译 stage04
3. 生成 rootfs        → 打包 busybox + ko + tools
4. run-arm64-qemu     → 启动 QEMU ARM64
5. smoke 自动执行     → /init 脚本触发 smoke test
```

### 8.2 成功标志

```
TX:  32 frames (RXIDX=0..31, SKBLEN=25, ETH=0x6865=0x88B7) ✅
RX:  32 POLL events (IDX=0..31, PROTO=0x88B7) ✅
NAPI poll 在 ARM64 上正常工作 ✅
端到端 smoke test 成功 ✅
```

### 8.3 关键验证点

- dmesg 有 `[stage04] TX RXIDX=0` 输出 → TX 路径通
- dmesg 有 `[stage04] POLL IDX=0` 输出 → RX + NAPI 路径通
- `recv` 工具显示 0 帧是**预期行为**（ethertype 0x88B7 无 handler，被 DROP）

---

## 九、四个关键技术修复总结

### 修复 1：CONFIG_NET 缺 symbol

**问题**：`register_netdev` undefined

**修复**：`.config` 加 `CONFIG_NET=y`、`CONFIG_NET_CORE=y`，重建 vmlinux + Image + modules

### 修复 2：CONFIG_PACKET 缺 AF_PACKET

**问题**：`Address family not supported by protocol`

**修复**：`.config` 加 `CONFIG_PACKET=m`，重建 af_packet.ko，注入 rootfs

### 修复 3：rootfs shebang 错误

**问题**：`Failed to execute /init (error -2)`

**修复**：`#!/busybox sh` → `#!/bin/sh`

### 修复 4：rootfs 缺目录

**问题**：`mount: mounting none on /proc failed: No such file or directory`

**修复**：打包前 `mkdir -p proc sys dev`

---

## 十、与 stage04 的关键差异

| 维度 | stage04 (x86_64 host) | stage06 (ARM64 + QEMU) |
|------|----------------------|------------------------|
| 编译器 | gcc（native） | aarch64-linux-gnu-gcc（cross） |
| KDIR | /lib/modules/$(uname -r)/build | ARM64 kernel build tree |
| 设备 | 模拟 device（PCI probe） | QEMU virt（PCI） |
| rootfs | native rootfs | busybox ARM64 initramfs |
| 模块加载 | modprobe（自动依赖） | insmod（手动顺序） |
| 符号表 | 原生 kernel vmlinux.symvers | ARM64 kernel vmlinux.symvers |
| init | 系统 init | QEMU initramfs /init |

---

## 十一、扩展方向（stage07+）

1. **smoke 集成到 Makefile**：`make smoke TARGET=qemu-arm64` 全自动
2. **rootfs 生成脚本化**：合并到 Makefile 一个 target
3. **records 同步**：把 wq7 的 smoke records 自动同步回本地
4. **回归测试**：ARM64 smoke test 加入 CI
5. **多平台矩阵扩展**：添加更多 ARM64 机型（raspi4 等）