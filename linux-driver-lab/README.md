# linux-driver-lab

Richer Wong的 Linux 驱动学习实验仓库

这个仓库的目标不是只放几段能跑的 demo，而是逐步沉淀成一个：

- 可复现的驱动学习项目
- 可持续扩展的 GitHub 知识库
- 可用于面试讲解的作品仓库

当前阶段聚焦 **W1：字符设备驱动基础闭环**。

---

## 1. 仓库结构总览

推荐按下面的目录放置：

```text
driver-lab/
├── kernel-src/
│   ├── README.md
│   ├── linux-5.15.10/
│   │   ├── README.md
│   │   └── .gitkeep
│   └── busybox-1.36.1/
│       ├── README.md
│       └── .gitkeep
└── linux-driver-lab/
    ├── README.md
    ├── docs/
    ├── day01/
    ├── day02/
    ├── day03/
    ├── day04/
    ├── day05/
    ├── day06/
    └── day07/
```

含义很简单：

- `linux-driver-lab/`：项目代码、脚本、README、学习记录
- `kernel-src/`：实验依赖环境占位目录
- GitHub 中只保留 `kernel-src/` 的目录骨架和说明文档，不提交完整内核源码与 BusyBox 源码

---

## 2. 为什么保留 kernel-src 目录骨架

这个仓库需要两个外部依赖：

- Linux 内核源码 `linux-5.15.10`
- BusyBox 源码 `busybox-1.36.1`

它们既是运行环境的一部分，又不适合整棵源码树直接提交到 GitHub。

所以这里采用折中方案：

- 保留 `driver-lab/kernel-src/` 目录骨架
- 在每个目录里放 `README.md` 说明如何下载、编译和验证
- 真正的大源码内容由使用者按说明自行准备

这样既能让仓库结构完整清晰，又不会把仓库变成超大包。

更详细的环境准备说明见：

- `../kernel-src/README.md`
- `docs/GITHUB_NOTES.md`

---

## 3. Windows 宿主机特别说明

如果你在 Windows + VMware + Ubuntu 的环境里跑 QEMU / KVM，最常见的问题不是驱动代码，而是宿主机虚拟化权限被 Hyper-V 占用了。

典型现象是：

- VMware 提示与 Hyper-V 冲突
- Ubuntu 里的 QEMU 没有硬件加速
- 虚拟机和内核实验明显变慢

建议按下面顺序排查：

1. 关闭 **内核隔离 / 内存完整性**
2. 在“启用或关闭 Windows 功能”里取消：
   - Hyper-V
   - 虚拟机平台
   - Windows 虚拟机监控程序平台
3. 进入 VMware 的 **虚拟机设置 -> 处理器**，勾选 **虚拟化 Intel VT-x/EPT 或 AMD-V/RVI(V)**
4. 以管理员身份执行：

```powershell
bcdedit /set hypervisorlaunchtype off
```

5. 重启物理机

勾选 VMware 里的“虚拟化引擎”这一步很关键。因为你的 Ubuntu 本身也是 VMware 里的 guest，而我们还要在 Ubuntu 里继续运行 QEMU/KVM。这个场景属于**嵌套虚拟化**。如果不把宿主 CPU 的虚拟化能力继续暴露给 Ubuntu，QEMU 只能退回到纯软件模拟，启动和调试都会明显变慢。

更完整的说明和截图放在 `../kernel-src/README.md` 里。

---

## 4. Ubuntu 环境准备

建议先安装实验所需基础工具：

```bash
sudo apt update
sudo apt install -y build-essential libncurses-dev bison flex libssl-dev libelf-dev cpio qemu-system-x86 wget xz-utils git
```

这些工具分别用于：

- 编译内核和外部模块
- 编译 BusyBox
- 打包 initramfs
- 启动 QEMU

---

## 5. Linux 内核源码与 BusyBox 准备

本仓库默认使用：

- `linux-5.15.10`
- `busybox-1.36.1`

下载与编译的完整步骤已经写入：

- `../kernel-src/linux-5.15.10/README.md`
- `../kernel-src/busybox-1.36.1/README.md`

准备完成后，应至少能看到这些关键产物：

### 内核侧

```text
kernel-src/linux-5.15.10/arch/x86/boot/bzImage
```

### BusyBox 侧

```text
kernel-src/busybox-1.36.1/busybox
```

如果执行过 `make install`，还可能有：

```text
kernel-src/busybox-1.36.1/_install/bin/busybox
```

---

## 6. build.sh 的默认路径规则

从这版开始，`day01` 到 `day06` 的 `build.sh` 优先按仓库相对路径寻找依赖：

```text
../kernel-src/linux-5.15.10
../kernel-src/busybox-1.36.1
```

如果仓库外仍保留旧路径：

```text
/home/wq7/workspace/kernel-src/linux-5.15.10
/home/wq7/workspace/kernel-src/busybox-1.36.1
```

脚本也会自动兼容回退。

你也可以手工指定：

```bash
KDIR=/path/to/linux-5.15.10 BUSYBOX_DIR=/path/to/busybox-1.36.1 ./build.sh
```

这样仓库在 GitHub 克隆到任意目录后都更容易复用。

---

## 7. 当前学习进度

### 已完成

- Day01：字符设备最小骨架
- Day02：ioctl SET/GET 与用户态测试
- Day03：sysfs 属性接口
- Day04：debugfs 状态快照与日志开关
- Day05：waitqueue / workqueue / 上下文
- Day06：回归脚本与压力测试，已完成 500 次装卸与 300 秒并发压测验收

### 当前收口项

- Day07：README 整理、环境安装文档补齐、W1 复盘

---

## 8. Day07 文档收口

Day07 不再新增驱动能力，重点是把仓库整理成“别人拿到后能照着搭环境并跑通”的形态。当前已经补齐：

- `day07/README.md`
- `docs/W1_REVIEW.md`
- `kernel-src/README.md` 及其子目录安装说明
- build.sh 的相对路径 + 旧路径兼容说明

建议先阅读：

1. `README.md`
2. `../kernel-src/README.md`
3. `day07/README.md`
4. `docs/W1_REVIEW.md`

## 9. 统一运行方式

进入某个 day 目录后执行：

```bash
chmod +x build.sh
./build.sh
```

说明：

- `chmod +x build.sh` 是给初学阶段保底
- 即使 Git 克隆后权限丢失，也能手动恢复执行权限
- `build.sh` 会自动准备 rootfs、打包 initramfs 并启动 QEMU

### Day06 推荐验收命令

宿主机：

```bash
cd day06
chmod +x build.sh
./build.sh
```

进入 guest 后：

```sh
/bin/all.sh
```

如果你想分步执行：

```sh
/bin/insmod_rmmod.sh 500
insmod /demo.ko
/bin/stress_rw.sh 300
/bin/check_dmesg.sh
```

---

## 10. waitqueue / workqueue 的简洁理解

### waitqueue

waitqueue 用于条件等待和阻塞唤醒。

它解决的是：

**现在条件不满足，先睡，等好了再叫醒我。**

常见场景：

- 阻塞式 `read()`
- 等待数据就绪
- 等待中断或 DMA 完成

### workqueue

workqueue 用于异步处理任务。

它解决的是：

**这个活先别在这里做，放到后台线程里再做。**

常见场景：

- 中断下半部
- 驱动异步处理命令
- 需要在进程上下文执行的工作

### 一起使用时

在当前 day05 / day06 的 demo 里：

- `write()` / `ioctl()` 提交任务
- `workqueue` 后台处理任务
- `read()` 通过 `waitqueue` 阻塞等待结果

可以一句话概括：

**workqueue 负责做事，waitqueue 负责等结果。**

### 餐馆类比

- 用户 `write()` / `ioctl()`：顾客下单
- `workqueue`：后厨做菜
- `waitqueue`：顾客坐着等叫号
- `read()`：顾客取餐

这个类比最容易记住。

---

## 11. GitHub 提交建议

建议提交：

- `.c / .h / Makefile / build.sh / README.md`
- `docs/` 下的学习文档与复盘
- `kernel-src/` 下的目录骨架与说明文档

不建议提交：

- 完整的 Linux 内核源码树
- 完整的 BusyBox 源码树
- `rootfs/`、`rootfs.img`
- `*.ko`、`*.o`、`Module.symvers`
- 编译生成的用户态二进制

更具体说明见 `docs/GITHUB_NOTES.md`。
