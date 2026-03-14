# Day16 LEARNING_PATH_01

## Day16 今天最合适的学习路径

Day16 不是“马上把内核变最小”，而是先建立一个判断框架：

- 哪些一定要保留
- 哪些明显可以第一轮先裁
- 哪些暂时先别碰

### 1. 先学“保留链路”

Day16 第一轮裁剪，必须保住这条最小闭环：

- QEMU virt 能启动到 shell
- initramfs 能工作
- `demo_regmap.ko` 能加载
- debugfs 可用
- tracing / function_graph 可用
- 后面还要给 Day17 预留 perf 内核侧能力

### 2. 再学“看启动日志识别可裁对象”

当前 baseline 启动日志里已经暴露出一批很像第一轮可裁候选的子系统：

- 多个网卡驱动：`e1000`、`e1000e`、`igb`、`igbvf`、`sky2`、`hns3`
- USB 主控与 USB 存储：`ehci_*`、`ohci_*`、`usb-storage`
- HID：`usbhid`
- ALSA 声音栈
- 一些当前 Day15 不依赖的外围驱动，如 `i2c_dev`、`sdhci` 相关堆栈

### 3. 学“第一轮先别动什么”

下面这些在 Day16 第一轮先不要碰：

- 串口控制台链路（PL011）
- 中断控制器 / 定时器
- Device Tree / platform driver 基础设施
- `proc` / `sysfs` / `devtmpfs`
- debugfs / tracefs / ftrace / function_graph
- `demo_regmap` 依赖的 MMIO / IRQ / workqueue 基础
- perf 内核侧配置
