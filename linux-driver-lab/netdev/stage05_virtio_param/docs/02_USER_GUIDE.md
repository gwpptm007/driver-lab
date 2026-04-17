# USER_GUIDE

## 快速开始

```bash
cd linux-driver-lab/netdev/stage05_virtio_param
make smoke
```

---

## 构建与运行流程

### 一键 smoke（完整验证入口）

```bash
make smoke
```

这会自动依次执行：

```
check_host_env          → 主机能力检测
resolve_platform_env    → host / x86_64+qemu / arm64+qemu 三个平台解析
collect_virtio_net_map  → 生成 virtio-net 源码阅读地图
generate_comparison_report → 生成 stage04 ↔ virtio-net 对照报告
generate_platform_matrix  → 生成平台矩阵
generate_stage05_report  → 生成阶段总报告
```

### 分步执行（按需）

```bash
make report              # 主机能力 + virtio_net.c 是否就绪
make virtio-map         # 生成 virtio-net.c 阅读地图（关键入口点）
make compare            # stage04 ↔ virtio-net 职责对照
make platform-matrix    # host / qemu-x86_64 / qemu-arm64 平台矩阵
```

### ARM64 平台单独验证

```bash
cd ~/workspace/driver-lab/linux-driver-lab/netdev/stage05_virtio_param
make resolve-platform TARGET_PROFILE=qemu-arm64
# 生成 output/resolved_qemu-arm64.env（含 KDIR / CROSS_COMPILE / QEMU_BIN 等）
```

---

## 验证步骤总览

| 步骤 | 命令 | 验证什么 | 预期结果 |
|------|------|---------|---------|
| 1 | `make report` | 主机工具体系 | gcc ✅ qemu ✅ aarch64-gcc ✅ virtio_net.c ✅ |
| 2 | `make virtio-map` | virtio-net 源码可读 | `output/virtio_net_map.md` 有 6 个关键函数入口 |
| 3 | `make compare` | stage04 ↔ virtio 对照 | `output/stage04_vs_virtio_report.md` 有对照表 |
| 4 | `make platform-matrix` | 三平台参数解析 | `output/platform_matrix.md` 三行平台记录 |
| 5 | `make smoke` | 全套 smoke | 全部 PASS |

---

## 产物与本地文件对应

所有产物在 `output/` 下：

| 产物文件 | 验证什么 |
|---------|---------|
| `output/stage05_report.md` | 工具链就绪、virtio_net.c 位置 |
| `output/virtio_net_map.md` | virtio-net 源码 6 个关键入口行号 |
| `output/stage04_vs_virtio_report.md` | 教学概念 → 真实实现 映射表 |
| `output/platform_matrix.md` | 三平台环境参数 |
| `output/resolved_*.env` | 各平台解析后的具体 env 值 |
| `output/host_env_*.env` | 主机检测到的实际环境 |

---

## 常见问题

### Q：smoke 报 `virtio_net.c: not found`

**原因**：`VIRTIO_NET_SOURCE` 环境变量未设置。

```bash
export VIRTIO_NET_SOURCE=/path/to/linux/drivers/net/virtio_net.c
make virtio-map
```

### Q：aarch64-linux-gnu-gcc command not found

```bash
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
```

### Q：qemu-system-aarch64 command not found

```bash
sudo apt install qemu-system-arm
```

### Q：ARM64 build 报 `MODPOST` 失败，符号找不到

**原因**：ARM64 kernel build 缺少 `vmlinux.symvers` 中的 netdev 符号。

**解决方法**：

```bash
cd /path/to/arm64/kernel/build

# 确认 CONFIG_NET=y
grep 'CONFIG_NET' .config

# 如果是 # CONFIG_NET is not set，改为：
sed -i 's/# CONFIG_NET is not set/CONFIG_NET=y/' .config
echo 'CONFIG_NET_CORE=y' >> .config

# 重新 build vmlinux + modules
sudo make vmlinux Image modules

# 验证符号出现
grep register_netdev vmlinux.symvers
```

---

## 文件同步

### 同步到测试机

```bash
rsync -av --delete \
  /e/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/ \
  wq7:workspace/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/
```

### 从测试机拉回

```bash
rsync -av wq7:workspace/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/output/ \
  /e/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/output/
```
