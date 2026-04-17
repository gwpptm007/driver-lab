# ACCEPTANCE & TASKS

## 验收标准

### 验收 1：TX / RX 全流程正确

证明：
- [ ] ARM64 交叉编译成功（`netdev_stage04.ko` 为 aarch64 ELF）
- [ ] 模块在 ARM64 上成功加载并注册 netdev（nds4）
- [ ] NAPI poll + ring DMA + RX replenishment 在 ARM64 上正常工作
- [ ] smoke test 端到端通过

### 验收 2：环境验收

证明 stage06 已摆脱个人机器路径依赖，能在准备好的新机器上复现。

- [ ] `resolve-host` 能在无个人路径前提下解析完成
- [ ] `resolve-x86` 能在无个人路径前提下解析完成
- [ ] `resolve-arm64` 能在无个人路径前提下解析完成
- [ ] `env/stage06_arm64_migration.env` 足以表达默认配置
- [ ] 缺失依赖时脚本能明确报错

### 验收 3：构建验收

- [ ] host 构建路径可独立执行
- [ ] arm64 构建路径可独立执行
- [ ] 模块产物进入固定输出目录
- [ ] 构建失败时能快速定位是环境问题还是源码问题

### 验收 4：运行时 smoke 验收

- [ ] 模块可加载
- [ ] netdev 注册成功
- [ ] 至少一轮 TX→RX 闭环成功
- [ ] NAPI poll 被触发
- [ ] RX replenish 正常发生
- [ ] dmesg 中无关键报错

### 验收 5：平台差异验收

- [ ] host / qemu-x86_64 / qemu-arm64 三个平台矩阵可生成
- [ ] stage04 ↔ stage06 差异报告可生成
- [ ] 一致项和差异项都有清晰解释

---

## 关键技术修复记录

### 1. ARM64 kernel build 缺网络符号（CONFIG_NET is not set）
- **问题**：vmlinux.symvers 缺少 register_netdev/dev_add_pack/free_netdev
- **原因**：原始 kernel .config 中 `# CONFIG_NET is not set`
- **修复**：
  1. `.config` 中设置 `CONFIG_NET=y`、`CONFIG_NET_CORE=y`
  2. `make vmlinux` 重新链接
  3. `make Image` 生成新 kernel Image
  4. `make modules` 生成完整 Module.symvers

### 2. CONFIG_PACKET 缺失
- **问题**：AF_PACKET sockets 不可用，报 "Address family not supported"
- **修复**：`CONFIG_PACKET=m`，重新 `make modules`，得到 af_packet.ko 注入 rootfs

### 3. rootfs init shebang 错误
- **问题**：`#!/busybox sh` → busybox 在 `/bin/busybox`，shebang 路径找不到
- **修复**：改为 `#!/bin/sh`

### 4. rootfs 缺少 /proc 和 /sys 目录
- **问题**：mount 失败
- **修复**：打包前 `mkdir -p proc sys dev`

---

## 迁移映射：什么不变 / 什么变化

### 保持不变
- `net_device` 生命周期
- `ndo_open` / `ndo_stop` / `ndo_start_xmit`
- NAPI 主逻辑（irq 触发 schedule、poll 按 budget 消费）
- ring / descriptor 的教学模型
- TX→RX 闭环的演示逻辑

### 发生变化的平台差异
| 差异维度 | host | ARM64 |
|---------|------|-------|
| 编译器 | gcc（native） | aarch64-linux-gnu-gcc（cross） |
| KDIR | /lib/modules/.../build | ARM64 kernel build tree |
| 模块加载 | modprobe | insmod（手动顺序） |
| rootfs | native rootfs | busybox ARM64 initramfs |
| init | 系统 init | QEMU initramfs /init |

---

## 通过标准

> **不依赖个人路径、不依赖人工记忆步骤、不把主驱动逻辑和平台脚手架混在一起。**

---

## 收口完成判据

只要下面 8 条全部满足，就可以把 stage06 判定为"已收口"：

1. 脚本中不再出现个人路径
2. host / x86 / arm64 三个平台都能 resolve
3. build 入口统一
4. dry-run 入口统一
5. smoke 分静态和运行两层
6. 验收 checklist 单独成文
7. migration mapping 单独成文
8. output / records / reports 边界清楚
