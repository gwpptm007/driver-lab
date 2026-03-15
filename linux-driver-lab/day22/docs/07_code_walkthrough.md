# day22 代码导读

## 1. 为什么这次要专门补一篇代码导读

因为这版 day22 的修正点很明确：

- 不能再只有脚本和文档
- 必须让 day22 开始就出现真正的 C 代码
- 而且这些代码要能直接服务 day23，而不是摆设

所以这篇文档只做一件事：

**告诉你 day22 应该先读哪两份代码，以及分别学什么。**

---

## 2. `tools/pci_sysfs_dump.c`

### 2.1 这份代码解决什么问题

它解决的是：

> 在还没写真正 `pci_driver` 之前，guest 里如何不用 `libpci`，也能观察 PCI 设备枚举结果。

### 2.2 读代码时重点关注什么

#### 关注点一：为什么直接走 sysfs

因为 day22 的重点还是“确认设备被看到”，而不是“重造一个 lspci”。

用 sysfs 的好处是：

- 最小 rootfs 更容易带起来
- 不需要额外库
- 更贴近驱动开发时常看的 `/sys/bus/pci/devices/*`

#### 关注点二：为什么要读 `resource`

因为 day23 要开始处理 BAR 映射。

`resource` 文件能让你在 day22 就先看到：

- 哪些 BAR 有效
- BAR 大概多大
- 是 MMIO 还是 I/O 资源

#### 关注点三：为什么还读 `config`

因为配置空间预览能证明：

- 目标设备不只是“目录存在”
- 而是 guest 里真的能访问 config 空间

---

## 3. `driver/day22_ivshmem_stub.c`

### 3.1 这份代码解决什么问题

它解决的是：

> day23 不要从零开始建 `pci_driver` 骨架，而是直接从 day22 这个 stub 往前推进。

### 3.2 读代码时重点关注什么

#### 关注点一：`pci_device_id`

先理解：

- 为什么匹配 `1af4:1110`
- 为什么 `MODULE_DEVICE_TABLE(pci, ...)` 要放进去

#### 关注点二：`probe/remove`

先建立生命周期感觉：

- 设备匹配成功后会进 `probe`
- 模块卸载或设备解绑时会进 `remove`

#### 关注点三：为什么 day22 只打印 BAR，不真正映射

因为今天还在 day22。

你应该刻意保留这个阶段边界：

- day22 看见 BAR
- day23 接管 BAR

这样后面学习节奏更清楚。

---

## 4. 推荐阅读顺序

1. 先读 `tools/pci_sysfs_dump.c`
2. 再读 `driver/include/day22_ivshmem_stub.h`
3. 再读 `driver/day22_ivshmem_stub.c`
4. 最后结合 `guest/init.day22` 看工具是怎么被自动调用的

---

## 5. 这篇代码导读的结论

day22 到这一步，才算真正进入“驱动作品线”的状态：

- 平台准备还在
- 自动化还在
- 证据归档还在
- 但已经不再只有脚本

而是已经开始有：

- guest 侧 C 工具
- 内核侧 `pci_driver` stub

这就是 day23 能顺利继续的关键。
