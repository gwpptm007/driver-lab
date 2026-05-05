# 03_TROUBLESHOOTING

## 1. `meson: command not found` 或 `ninja: command not found`

**原因**: DPDK 21.11+ 使用 meson+ninja 构建系统，而非传统的 make。

DPDK 演进历程:
```
make (DPDK ≤ 20.x) → meson + ninja (DPDK 21.11+)
```

**meson**: 替代 Makefile 的声明式构建配置工具，比 make 更快速、更可靠
**ninja**: 替代 make 的高性能构建执行器，配合 meson 使用

编译流程:
```bash
meson setup build          # 生成构建配置 (build.ninja)
ninja -C build             # 执行构建
```

**安装方式**:
```bash
sudo apt update
sudo apt install -y meson ninja-build
```

## 2. `pkg-config --exists libdpdk` 失败

说明开发包缺失。

常见安装方式：

```bash
sudo apt update
sudo apt install -y dpdk dpdk-dev libdpdk-dev meson ninja-build pkg-config build-essential
```

## 3. `rte_eal_init failed`

常见原因：

```text
hugepage 没配置
file-prefix 冲突
没有权限访问 hugepage/vfio/uio
PCI 设备没有绑定到 DPDK driver
EAL 参数顺序错误
```

先执行：

```bash
./scripts/00_check_env.sh
sudo ./scripts/02_prepare_vmxnet3.sh
```

## 4. `no available DPDK ethdev ports found`

说明 EAL 成功，但没有可用 ethdev。

检查：

```bash
dpdk-devbind.py --status
```

确认：

```text
0000:0b:00.0 drv=uio_pci_generic
```

并且运行命令里有：

```bash
-a 0000:0b:00.0
```

## 5. `vfio-pci` 失败

当前 VMware Workstation guest 通常没有完整 IOMMU，`vfio-pci` 失败是预期内问题。

本 lab 默认使用：

```text
uio_pci_generic
```

## 6. RX/TX 一直是 0

当前测试机没有外部发包源，且只有一个 DPDK 口，RX/TX 为 0 不影响 `PASS_SMOKE`。

如果要验证真实转发，需要：

```text
第二个 DPDK 口
或外部发包器
或 vhost/virtio-user 拓扑
```

## 7. SSH 断开风险

脚本内置保护：

```text
DPDK_PCI 不允许等于 MGMT_PCI
```

默认：

```text
MGMT_PCI=0000:02:01.0
DPDK_PCI=0000:0b:00.0
```

不要把 ens33 绑定到 DPDK driver。
