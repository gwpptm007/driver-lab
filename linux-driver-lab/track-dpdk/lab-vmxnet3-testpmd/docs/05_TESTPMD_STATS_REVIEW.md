# 05_TESTPMD_STATS_REVIEW

## testpmd 在本 Lab 中的意义

`testpmd` 是 DPDK 自带的测试和验证工具。本 Lab 用它验证：

```text
EAL 能启动
hugepage 能使用
PCI 设备能被识别
PMD 能 probe（Poll Mode Driver）
port 能初始化
队列能配置
stats 能输出
```

它不是最终项目代码，但它是写 C app 前的**环境基线验证**。

## testpmd 关键参数解析

```bash
dpdk-testpmd -l 0-1 -n 4 -a 0000:0b:00.0 -- --forward-mode=io --auto-start --stats-period=5
```

| 参数 | 含义 |
|------|------|
| `-l 0-1` | 使用 logical cores 0 和 1 |
| `-n 4` | 4 memory channels |
| `-a 0000:0b:00.0` | 添加 PCI 设备到 allow list |
| `--forward-mode=io` | I/O 转发模式（RX → TX） |
| `--auto-start` | 启动时自动开始转发 |
| `--stats-period=5` | 每 5 秒输出统计 |

## 需要重点看什么

在 `TESTPMD.log` 中重点找：

```
EAL: Selected IOVA mode 'PA'      # IOVA 模式（PA=物理地址，VA=虚拟地址）
EAL: VFIO support initialized     # VFIO 初始化状态
EAL: Probe PCI driver: net_vmxnet3  # PMD 探测到设备
Configuring Port 0                 # 端口配置
Port 0: 00:0C:29:F8:F6:82       # MAC 地址
Checking link statuses...         # 链路状态
io packet forwarding              # 转发模式
NIC statistics for port 0        # 端口统计
```

**成功标志**：

- ✅ `EAL: Probe PCI driver: net_vmxnet3 ... device: 0000:0b:00.0 (socket 0)`
- ✅ `testpmd: create a new mbuf pool <mb_pool_0>`
- ✅ `Configuring Port 0`
- ✅ `Port 0: XX:XX:XX:XX:XX:XX`

## IOVA 模式说明

```
IOVA = I/O Virtual Address

PA 模式：物理地址，DMA 需要
VA 模式：虚拟地址，更灵活但需要 IOMMU
```

在 VMware 环境下，由于没有 IOMMU，`testpmd` 会自动选择 **PA 模式**。

## mbuf pool 是什么

mbuf（memory buffer）是 DPDK 的数据包描述符结构：

```
mbuf pool: n=155456, size=2176, socket=0
```

| 字段 | 含义 |
|------|------|
| `n=155456` | pool 中 mbuf 数量（约 15 万） |
| `size=2176` | 每个 mbuf 大小（2KB+） |
| `socket=0` | 位于 NUMA node 0 |

**为什么需要大页**：155456 × 2KB ≈ 300MB，单个大页池减少 TLB miss。

## 没有外部流量是否算失败

不一定。

第一轮的最低目标是 `testpmd` 能启动并输出 port/stats。如果没有对端向 `ens192` 所在网络发包，RX/TX 计数是 0，这不影响"**环境闭环**"判定。

**后续增强方式**：

| 方式 | 说明 |
|------|------|
| scapy | 另一台机器发送 UDP 包 |
| pktgen | DPDK 官方发包工具 |
| VM间组网 | VMware hostonly 网络 |
| 回环测试 | 用 `eth loopback` 或内部回环 |

但那属于下一轮增强，本 Lab 目标达成即可。

## 当前 Lab 的复盘问题

完成后需要能回答：

**1. `ens192` 为什么 bind 后从 Linux 网络栈消失？**

绑定到 `uio_pci_generic` 后，设备从 kernel 驱动脱离，由 DPDK 用户空间驱动接管。Linux 网络栈不再管理这个设备，所以 `ip link` 和 `ifconfig` 看不到它。

**2. `vmxnet3` 内核驱动和 DPDK vmxnet3 PMD 有什么区别？**

| 项目 | kernel vmxnet3 | DPDK vmxnet3 PMD |
|------|---------------|------------------|
| 位置 | 内核空间 | 用户空间 |
| 数据路径 | kernel →协议栈→app | 直接 DMA → app |
| 性能 | 受限于 kernel | 零拷贝，接近线速 |
| 用途 | 普通网络 | DPDK 高性能 |

**3. hugepage 对 mbuf/mempool 有什么价值？**

- **减少 TLB miss**：2MB 页 vs 4KB 页
- **保证物理连续**：DMA 需要物理连续内存
- **降低页表开销**：大池子用少量大页更高效

**4. `testpmd` 成功说明了什么，没说明什么？**

✅ 说明了：
- EAL 初始化正常
- hugepage 可用
- PCI 设备被 DPDK 识别
- PMD 能 probe 和初始化
- 端口能收发包

❌ 没说明：
- 实际吞吐量性能
- 多核扩展性
- 与对端设备的实际通信
- 复杂转发逻辑

**5. 为什么下一步是 vhost-user，而不是马上做复杂业务？**

vhost-user 是 DPDK 与 QEMU/virto-net 的标准接口，比直接写 NIC 应用更接近真实场景：
- 理解 virtio frontend/backend 架构
- 掌握 vhost-user socket 通信
- 为 later L2 forwarding 打基础
