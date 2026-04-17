# USER_GUIDE

## 快速开始

### 阅读顺序

1. `docs/01_STAGE_OVERVIEW.md` — 目标、迁移策略、平台路径
2. `docs/02_USER_GUIDE.md` — 使用方式、build/run、常见问题
3. `docs/03_ACCEPTANCE.md` — 验收标准和检查单
4. `docs/04_DEEP_LEARNING.md` — 深度分析
5. `include/netdev_kcompat.h` — 兼容层代码

### 测试流程

```bash
# 环境解析
make resolve-host
make resolve-x86
make resolve-arm64

# 构建
make build-host
make build-arm64

# dry-run 和 smoke
make dryrun-arm64
make smoke

# 报告
make report
make matrix
make diff
```

---

## 构建与运行流程

### host 路径
适合：先跑脚本、先检查 KDIR、先尝试 stage04 模块构建
```bash
make resolve-host
make build-stage04-host
```

### qemu-x86_64 路径
适合：先把 QEMU run 习惯和 env 参数整理出来，不急着上 ARM64 时做过渡验证
```bash
make resolve-x86
make matrix
```

### qemu-arm64 路径
适合：完成正式迁移，输出真正的迁移收口报告
```bash
make resolve-arm64
make dryrun-arm64
make build-stage04-arm64
```

### 为什么先 dry-run
QEMU/ARM64 最容易卡在路径、参数、镜像、工具链。先把命令行和环境解析生成出来，再去真机执行，可以把问题压缩到最小。

---

## ARM64 smoke test 验证（2026-04-13）

```
TX:  32 frames (RXIDX=0..31, SKBLEN=25, ETH=0x6865=0x88B7) ✅
RX:  32 POLL events (IDX=0..31, PROTO=0x88B7) ✅
NAPI poll 在 ARM64 上正常工作 ✅
端到端 smoke test 成功 ✅
```

---

## 常见问题

### Q1：MODPOST 报 `register_netdev undefined`

**原因**：ARM64 kernel 的 `vmlinux.symvers` 缺少网络符号。

**修复**：
```bash
sed -i 's/# CONFIG_NET is not set/CONFIG_NET=y/' $KDIR/.config
echo 'CONFIG_NET_CORE=y' >> $KDIR/.config
sudo make -C $KDIR vmlinux Image modules
grep register_netdev $KDIR/vmlinux.symvers
```

### Q2：QEMU 启动后 `/init` 报错 `Failed to execute /init (error -2)`

**原因**：shebang 路径错误。

**修复**：确保 `#!/bin/sh`（不是 `#!/busybox sh`）。

### Q3：`ip link` 报错 `Address family not supported by protocol`

**原因**：`CONFIG_PACKET` 未启用。

**修复**：`CONFIG_PACKET=m`，重新编译 af_packet.ko 并注入 rootfs。

### Q4：`mount: mounting none on /proc failed: No such file or directory`

**原因**：busybox rootfs 默认没有 `/proc` 目录。

**修复**：`mkdir -p proc sys dev` 在打包前。

### Q5：recv 收到 0 帧，但 dmesg 显示 POLL 收到 32 帧

**原因**：`eth_type_trans` 把 ethertype 0x88B7 发到内核协议栈，没有注册 handler 则 DROP。这是**预期行为**。

---

## 产物与本地文件对应

| 产物文件 | 本地对应路径 | 验证什么 |
|---------|------------|---------|
| `output/netdev_stage04.ko` | `.../stage06_.../output/` | ARM64 模块 |
| `output/rootfs_arm64.img` | `.../stage06_.../output/` | 含 smoke test 的 rootfs |
| `output/platform_matrix.md` | `.../stage06_.../output/` | 三平台参数 |
| `records/20260413-arm64-smoke/` | `.../stage06_.../records/` | smoke 结果记录 |

---

## 兼容层代码

### `include/netdev_kcompat.h`
- NAPI 兼容包装
- `u64_stats` 兼容包装
- 常用内核版本判断宏

### `include/netdev_stage_port_profile.h`
- host / x86_64 / arm64 的推荐默认参数
- ring / napi_weight / rx_buf_size 的平台建议

---

## 阶段计划

1. 先把 host / x86_64 / ARM64 参数解析做对
2. 再确保至少一条 ARM64 build 路径可执行
3. 再做 QEMU dry-run 命令生成
4. 最后在真实测试机上完成 run / smoke / records 收口
