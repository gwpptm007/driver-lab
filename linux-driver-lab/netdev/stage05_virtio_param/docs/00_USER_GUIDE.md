# stage05 使用指南

## 1. stage05 是做什么的

stage05 的核心是**两件事**，不写新代码：

| 目标 | 说明 |
|------|------|
| virtio-net 源码对照 | 把 stage04 的教学概念（ring/DMA/NAPI/RX replenishment）映射到真实驱动 `virtio-net` 的实现上 |
| 平台参数化准备 | 把 ARM64 迁移需要的环境差异（ARCH / CROSS_COMPILE / KDIR / QEMU_BIN 等）收敛到统一的 env 层 |

**一句话总结**：把 stage04 的"教学坐标系"升级成能与 `virtio-net` 对话、并能迁移到多平台的"工程化坐标系"。

---

## 2. 在测试机上执行

### 2.1 环境准备（wq7）

```bash
# 1. 同步代码到测试机
rsync -av --delete \
  /e/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/ \
  wq7:workspace/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/

# 2. 连接测试机
ssh wq7
```

### 2.2 一键 smoke（完整验证入口）

```bash
cd ~/workspace/driver-lab/linux-driver-lab/netdev/stage05_virtio_param
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

### 2.3 分步执行（按需）

```bash
make report              # 主机能力 + virtio_net.c 是否就绪
make virtio-map         # 生成 virtio_net.c 阅读地图（关键入口点）
make compare             # stage04 ↔ virtio-net 职责对照
make platform-matrix     # host / qemu-x86_64 / qemu-arm64 平台矩阵
```

### 2.4 ARM64 平台单独验证

```bash
cd ~/workspace/driver-lab/linux-driver-lab/netdev/stage05_virtio_param
make resolve-platform TARGET_PROFILE=qemu-arm64
# 生成 output/resolved_qemu-arm64.env（含 KDIR / CROSS_COMPILE / QEMU_BIN 等）
```

---

## 3. 验证每一步

### 3.1 验证步骤总览

| 步骤 | 命令 | 验证什么 | 预期结果 |
|------|------|---------|---------|
| 1 | `make report` | 主机工具体系 | gcc ✅ qemu ✅ aarch64-gcc ✅ virtio_net.c ✅ |
| 2 | `make virtio-map` | virtio-net 源码可读 | `output/virtio_net_map.md` 有 6 个关键函数入口 |
| 3 | `make compare` | stage04 ↔ virtio 对照 | `output/stage04_vs_virtio_report.md` 有对照表 |
| 4 | `make platform-matrix` | 三平台参数解析 | `output/platform_matrix.md` 三行平台记录 |
| 5 | `make smoke` | 全套 smoke | 全部 PASS |

### 3.2 每一步的验证逻辑

#### 步骤 1：report —— 工具链就绪

检查本机是否有：
- `gcc`：host 构建能力
- `qemu-system-x86_64` / `qemu-system-aarch64`：QEMU 虚拟化
- `aarch64-linux-gnu-gcc`：ARM64 交叉编译
- `virtio_net.c` 路径：源码对照的原材料

**验证文件**：`output/stage05_report.md`

```
- Host kernel: 6.8.0-107-generic
- gcc available: yes
- virtio_net.c found: yes
- virtio_net.c path: /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/drivers/net/virtio_net.c
```

#### 步骤 2：virtio-map —— virtio-net 源码阅读地图

从 `VIRTIO_NET_SOURCE` 指向的源码中抓取 6 个关键函数的行号和调用路径，生成阅读导航。

**验证文件**：`output/virtio_net_map.md`

```markdown
### probe
3073:static int virtnet_probe(struct virtio_device *vdev)

### xmit
1680:static netdev_tx_t start_xmit(struct sk_buff *skb, struct net_device *dev)

### poll
1525:static int virtnet_poll(struct napi_struct *napi, int budget)

### refill
1317:static bool try_fill_recv(struct virtnet_info *vi, struct receive_queue *rq, ...)
```

如果 `virtio_net_map.md` 显示 `virtio_net.c: not found`，需要设置：

```bash
export VIRTIO_NET_SOURCE=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/drivers/net/virtio_net.c
make virtio-map
```

#### 步骤 3：compare —— stage04 ↔ virtio-net 对照报告

对照两者在 TX / RX / NAPI / DMA / RX replenishment 五个维度的实现差异。

**验证文件**：`output/stage04_vs_virtio_report.md`

核心对照表：

| stage04 教学实现 | virtio-net 对应 | 结论 |
|---|---|---|
| tx_ring / rx_ring | send / receive virtqueue | 不能原样照抄 |
| owner/state 显式字段 | avail / used ring 协议 | 要从字段思维切到 ring 协议思维 |
| memcpy 模拟 device copy | 真实 buffer 提交与完成 | 教学模型结束 |
| dma_map_single/unmap 显式 | transport + sg + DMA 抽象 | 不能按 stage04 代码形状去找 |
| refill_rx_slot | try_fill_recv | 核心问题不变 |

#### 步骤 4：platform-matrix —— 三平台参数矩阵

解析 host / qemu-x86_64 / qemu-arm64 三个平台的：
- `TARGET_ARCH`：`host` / `x86_64` / `arm64`
- `RUN_MODE`：`host` / `qemu-x86_64` / `qemu-arm64`
- `CROSS_COMPILE`：空 或 `aarch64-linux-gnu-`
- `QEMU_BIN`：`/usr/bin/qemu-system-aarch64` 等
- `KDIR`：kernel build 目录

**验证文件**：`output/platform_matrix.md`

```markdown
| profile | arch | run mode | qemu | cross toolchain | kernel build dir |
|---|---|---|---|---|---|
| host | host | host | n/a | native gcc | /lib/modules/6.8.0-107-generic/build |
| qemu-x86_64 | x86_64 | qemu-x86_64 | /usr/bin/qemu-system-x86_64 | native gcc | n/a |
| qemu-arm64 | arm64 | qemu-arm64 | /usr/bin/qemu-system-aarch64 | aarch64-linux-gnu- | /home/wq7/workspace/.../build/arm64 |
```

#### 步骤 5：smoke —— 一键全套

执行 `scripts/smoke.sh`，依次调用以上四步，汇总结果到 `output/stage05_report.md`。

---

## 4. 产物与本地文件的对应关系

所有产物在**测试机 wq7** 的 `~/workspace/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/output/` 下，验证说明如下：

| 产物文件 | 本地对应路径（clone 后） | 验证什么 |
|---------|------------------------|---------|
| `output/stage05_report.md` | `linux-driver-lab/netdev/stage05_virtio_param/output/stage05_report.md` | 工具链就绪、virtio_net.c 位置 |
| `output/virtio_net_map.md` | `linux-driver-lab/netdev/stage05_virtio_param/output/virtio_net_map.md` | virtio-net 源码 6 个关键入口行号 |
| `output/stage04_vs_virtio_report.md` | `linux-driver-lab/netdev/stage05_virtio_param/output/stage04_vs_virtio_report.md` | 教学概念 → 真实实现 映射表 |
| `output/platform_matrix.md` | `linux-driver-lab/netdev/stage05_virtio_param/output/platform_matrix.md` | 三平台环境参数 |
| `output/resolved_*.env` | `linux-driver-lab/netdev/stage05_virtio_param/output/resolved_*.env` | 各平台解析后的具体 env 值 |
| `output/host_env_*.env` | `linux-driver-lab/netdev/stage05_virtio_param/output/host_env_*.env` | 主机检测到的实际环境 |

产物同步回本地：

```bash
# 在本地机器执行（Windows/macOS/Linux）
rsync -av wq7:workspace/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/output/ \
  /e/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/output/
```

---

## 5. 原理图：我们学到了什么

### 5.1 整体认知升级

```
stage04（教学模型）                    stage05/virtio-net（真实实现）
─────────────────────────────────      ─────────────────────────────────
单文件 driver/netdev_stage04.c         drivers/net/virtio_net.c（1600+ 行）
├─ 单一 tx_ring / rx_ring 数组         ├─ send_queue / receive_queue + virtqueue
├─ owner/state 显式字段管理             ├─ avail_idx / used_idx ring 协议
├─ dma_map_single 每包显式映射         ├─ virtio transport 隐藏 DMA 细节
├─ raise_irq + napi_schedule           ├─ virtqueue callback → napi_schedule
├─ refill_rx_slot 同步在 poll 内        ├─ try_fill_recv 批量异步填充
└─ memcpy 模拟 device 行为             └─ 真实 virtio device（QEMU 模拟）
```

### 5.2 五个关键对照

#### RX Replenishment（RX buffer 不断粮）

```
stage04:  poll() 内每处理一个包，立刻 refill 对应 slot
         ┌─ 同步、低延迟、简单
         └─ 教学目的：理解 RX "不断粮" 的本质

virtio:   try_fill_recv() 在 poll 结束后批量填充
         ┌─ 批量操作、减少通知次数
         └─ 真实场景：减少 virtqueue kick 开销
```

#### TX 路径

```
stage04:  ndo_start_xmit → skb_linearize → dma_map_single → memcpy → DONE
virtio:  ndo_start_xmit → virtio_net_hdr_from_skb → virtqueue_add_outbuf → virtqueue_notify
                                                                                  ↓
                                                                         QEMU virtio-net backend
```

#### DMA 操作

```
stage04:  driver 显式调用 dma_map_single/unmap_single
         ┌─ 路径清晰、适合学习
         └─ 每包操作，有一定 CPU 开销

virtio:  virtqueue_add_* → virtio_ring.c 处理 sg list → transport 层 DMA
         ┌─ 分层抽象、driver 不直接碰 DMA
         └─ 真实硬件：pci-skeleton / virtio-mmio 处理
```

#### NAPI Poll

```
stage04:  单一 napi_struct，轮询单一 rx_ring
virtio:  每个 virtqueue 一个 napi_struct（send_queue + receive_queue）
         └─ 支持多队列网卡的高性能模型
```

#### ownership 协议

```
stage04:  owner = CPU | DEV  显式字段
         ┌─ 简单直观
         └─ 教学用：完全受 driver 控制

virtio:  avail->idx（driver 写）vs used->idx（device 写）环形协议
         ┌─ 无锁设计、共享内存通信
         └─ 真实 hypervisor / device 协同模型
```

### 5.3 平台参数化架构

```
env/stage05_virtio_param.env          ← 默认值（host 假设）
         ↓
scripts/resolve_platform_env.sh      ← 根据 TARGET_PROFILE 选择分支
         ↓
output/resolved_*.env                ← 解析后的具体 env 值
         ↓
build_stage04_for_target.sh          ← 用 resolved env 驱动 stage04 交叉编译
```

---

## 6. 常见问题

### Q：smoke 报 `virtio_net.c: not found`

**原因**：`VIRTIO_NET_SOURCE` 环境变量未设置。

```bash
export VIRTIO_NET_SOURCE=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/drivers/net/virtio_net.c
make virtio-map
```

### Q：aarch64-linux-gnu-gcc command not found

```bash
# 在 wq7 上安装
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
```

### Q：qemu-system-aarch64 command not found

```bash
# 在 wq7 上安装
sudo apt install qemu-system-arm
```

### Q：ARM64 build 报 `MODPOST` 失败，符号找不到

**原因**：ARM64 kernel build 缺少 `vmlinux.symvers` 中的 netdev 符号。

**解决方法**（在 wq7 上执行）：

```bash
cd /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64

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

### Q：本地文件如何同步回测试机

```bash
# 本地 → wq7
rsync -av /e/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/ \
  wq7:workspace/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/

# wq7 → 本地
rsync -av wq7:workspace/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/output/ \
  /e/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/netdev/stage05_virtio_param/output/
```
