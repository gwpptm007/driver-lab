# day23 详细计划

## 1. 今日主题
pci_driver 骨架与 BAR 映射

## 2. 核心目标
实现 `pci_driver` 的 probe/remove 基础骨架，打通 `enable_device/request_regions/pci_iomap`。

## 3. 今日最小闭环
- 输入：前置环境、设备后端、源码目录、guest 工具
- 过程：实施步骤、观察日志、校验行为
- 输出：原始证据、阶段结论、下一步输入

## 4. 实施步骤展开

### 1. 步骤 1
编写最小 `pci_driver` 骨架与 `MODULE_DEVICE_TABLE`。

### 2. 步骤 2
在 probe 中依次完成：`pci_enable_device()`、`pci_request_regions()`、`pci_set_master()`、`pci_iomap()`。

### 3. 步骤 3
打印 BAR 起止地址、长度、虚拟映射地址。

### 4. 步骤 4
在 remove 中对称释放资源并验证重复加载。


## 5. 建议当天保留的证据
- 命令原文
- 关键日志
- 错误样本
- 最终结论

## 6. 当天结束前自查
- 主链路是否完成
- 异常路径是否至少验证一条
- `records/` 是否可供别人复盘
