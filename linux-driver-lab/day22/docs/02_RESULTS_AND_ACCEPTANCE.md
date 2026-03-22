# 02_RESULTS_AND_ACCEPTANCE：day22 最终结果与验收说明

这份文档只回答两件事：

1. day22 到底算不算通过；
2. 需要拿什么证据证明通过。

---

## 1. 最终结论

**结论：day22 核心通过。**

当前真实运行结果已经证明：

- PCI host bridge 初始化成功；
- `ivshmem` 设备 `1af4:1110` 已被成功枚举；
- `lspci -vv -nn` 已输出设备 BAR 信息；
- guest 自动流程已经跑到 `===DAY22:COMPLETE===`；
- QEMU 侧没有额外致命错误。

因此，从 D22 的学习目标和主验收目标看，可以判定为：

> **day22 核心通过，工程收口未完成。**

---

## 2. 通过的核心证据是什么

### 证据 1：PCI host bridge 成功初始化

在 `serial.log` 中应能看到类似：

- `pci-host-generic ... PCI host bridge to bus 0000:00`
- `pci_bus 0000:00: root bus resource ...`

这说明 guest 内部的 PCI 总线真的起来了。

### 证据 2：ivshmem 设备被成功枚举

在 `lspci -nn` 或串口日志对应段落中，应能看到：

- `00:02.0 Class [0500]: Device [1af4:1110] (rev 01)`

这就是 D22 最关键的一条验收证据。

### 证据 3：`lspci -vv -nn` 已输出 BAR 信息

在 `lspci -vv -nn` 段落中，应能看到：

- `Region 0: Memory at ... [size=256]`
- `Region 2: Memory at ... [size=4M]`

这说明设备不只是“被看见”，而且 PCI 资源也被枚举并打印出来了。

### 证据 4：guest 自动流程跑完

在 `serial.log` 中应能看到：

- `===DAY22:COMPLETE===`

这说明 guest 侧自动测试流程已经完整执行到结束。

---

## 3. 最终应该看哪些文件

建议按下面优先级判断：

### 第一优先级

- `records/${RUN_ID}/serial.log`
- `records/${RUN_ID}/lspci-nn.txt`
- `records/${RUN_ID}/lspci-vv-nn.txt`
- `records/${RUN_ID}/dmesg-pci.txt`

### 第二优先级

- `records/${RUN_ID}/sysfs-pci-devices.txt`
- `records/${RUN_ID}/qemu.stderr.log`

### 谨慎使用

- `records/${RUN_ID}/run-summary.md`

当前版本里，`run-summary.md` 仍可能出现 **false negative**，也就是脚本写“否”，但实际串口日志已经成功。

---

## 4. 建议采用的最终验收表

| 验收项 | 当前判定 | 应看证据 |
|---|---|---|
| PCI host bridge 初始化成功 | 通过 | `serial.log` 中 `pci-host-generic ... PCI host bridge to bus 0000:00` |
| ivshmem 设备可见 | 通过 | `lspci -nn` 中 `00:02.0 ... [1af4:1110]` |
| `lspci -vv -nn` 已输出 | 通过 | `lspci-vv-nn.txt` 或 `serial.log` 中对应段落 |
| BAR 信息可见 | 通过 | `Region 0` / `Region 2` |
| PCI dmesg 已归档 | 通过 | `dmesg-pci.txt` 或 `serial.log` 中 `DMESG_PCI` 段 |
| guest 自动流程跑完 | 通过 | `===DAY22:COMPLETE===` |
| `pci_sysfs_dump` 辅助工具 | 未收口 | guest 内执行失败 |
| PCI config 样本 | 未收口 | `/init` 中 `head` 缺失 |
| PCI stub 模块 | 延后到 day23 | `make module` 仍失败 |

---

## 5. 当前剩余问题怎么写

这些问题建议在 day22 收口版中保留说明，但不要拿来否定 day22 主结果：

### 问题 A：`run-summary.md` 误判

当前脚本可能把成功结果写成“否”。

处理建议：
- 以 `serial.log` marker 为准；
- 后续修 `extract_records/run-summary` 逻辑。

### 问题 B：`pci_sysfs_dump` 在 guest 内执行失败

目前它没有影响“设备可见性验证”主目标，但确实说明辅助工具打包/执行链路还需修。

### 问题 C：`head` 缺失导致 PCI config 样本不完整

这不影响 D22 主结论，但会影响辅助证据完整性。

### 问题 D：`make module` 失败

当前更适合作为 day23 的前置问题处理，而不是 day22 的失败标准。

---

## 6. 推荐在最终 README 中写的一句话

> day22 已完成 QEMU `virt` 平台下 PCI 设备可见性验证：`ivshmem` 设备 `1af4:1110` 已成功枚举，`lspci -nn / -vv -nn`、PCI 相关 `dmesg` 和 BAR 信息均已取得证据，且 guest 自动流程已跑到 `===DAY22:COMPLETE===`。因此，day22 的核心学习目标与主验收目标已经达成；当前剩余问题主要集中在辅助脚本与工具收口，不影响将本轮结果判定为“核心通过”。
