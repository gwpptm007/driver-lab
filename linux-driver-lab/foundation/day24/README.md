# day24：MMIO 读写与共享内存协议（最终版）

## 1. 今日完成了什么

`day24` 在 `day23`“驱动已成功接住 ivshmem 设备”的基础上，进一步完成了一个**最小但真实可验证的 MMIO + 共享内存协议闭环**：

- 继续保留 `pci_enable_device / pci_request_regions / pci_iomap`
- 在 **BAR2 共享内存窗口** 起始位置定义协议头
- 协议头字段通过 `readl/writel` 访问
- payload 区域通过 `memcpy_toio / memcpy_fromio` 访问
- 提供字符设备 `/dev/day24_ivshmem0`
- 提供用户态工具 `day24_mmio_tool`
- 在 guest 内自动完成：
  - 模块加载
  - `info` 信息读取
  - `mmio-read`
  - `mmio-write`
  - `shm-write`
  - `shm-read`
  - 模块卸载

## 2. 最终结论

**day24 通过。**

真实运行输出已经证明：

- 设备 `1af4:1110` 被成功枚举
- 驱动 `probe()` 成功
- BAR0/BAR2 资源识别正确
- BAR2 协议头初始化成功
- `mmio-write` 修改 `state` 字段后，可被 `mmio-read` 读回
- `shm-write` 写入 payload 后，可被 `shm-read` 原样读回
- `rmmod` 成功
- guest 流程运行到 `===DAY24:COMPLETE===`

## 3. day24 的独立构建原则

- 只使用 `day24/` 目录内的脚本、文档、工具和环境文件
- 不依赖 `day22/` 或 `day23/` 的脚本、记录或中间产物
- `driver-lab/kernel-src/` 不随包提供，视为你本机已有前置
- `pciutils` 源码也不随包提供；**获取命令、编译命令、验证命令已经写进 day24 主流程**

## 4. 本地主流程（从获取 pciutils 源码开始）

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day24
source env/local.wq7.env

mkdir -p third_party
# 能联网时：
git clone https://github.com/pciutils/pciutils.git third_party/pciutils
# 不能联网时：把其它机器上的 pciutils 源码离线拷到 third_party/pciutils

chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true

make build-lspci
file third_party/pciutils/lspci

make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run
```

## 5. day24 通过后最应该看什么

1. `records/<RUN_ID>/run-summary.md`
2. `records/<RUN_ID>/mmio-info.txt`
3. `records/<RUN_ID>/mmio-read-before.txt`
4. `records/<RUN_ID>/mmio-write-state.txt`
5. `records/<RUN_ID>/mmio-read-after.txt`
6. `records/<RUN_ID>/shm-write.txt`
7. `records/<RUN_ID>/shm-read.txt`
8. `records/<RUN_ID>/dmesg-driver.txt`
9. `records/<RUN_ID>/lspci-vv-nn.txt`
10. `records/<RUN_ID>/serial.log`

## 6. 只看哪些文档就够了

1. `START_HERE.md`
2. `env/local.wq7.env`
3. `docs/01_LOCAL_RUNBOOK.md`
4. `docs/02_RESULTS_AND_ACCEPTANCE.md`
5. `docs/03_TROUBLESHOOTING.md`
6. `output/day24_quick_commands.md`

## 7. 下一步

如果 day24 通过，下一步就是 `day25`：MSI / `pci_alloc_irq_vectors()`。
