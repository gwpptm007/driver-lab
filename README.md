# driver-lab

一个面向初学者的 Linux 驱动学习项目。

本仓库以“字符设备驱动 + QEMU 实验环境 + 分天推进”的方式，逐步练习 Linux 内核模块开发、阻塞读写、ioctl、waitqueue、workqueue、回归测试与压力测试，并整理配套文档，保证代码和环境都能复现。

---

## 仓库目标

这个仓库主要解决两个问题。

1. **代码怎么学**
   - 按 day01 ~ day07 逐步推进
   - 每天一个明确目标
   - 每天都有代码、脚本、说明和验收点

2. **环境怎么搭**
   - 提供 `kernel-src/` 目录骨架
   - 明确 Linux 内核源码与 BusyBox 的准备过程
   - 说明 Windows + VMware + Ubuntu + QEMU 的实验链路
   - 尽量保证别人 clone 后可以按文档复现

---

## 顶层目录说明

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

### 目录含义

- `kernel-src/`
  - 放实验依赖环境
  - 包括 Linux 内核源码目录和 BusyBox 源码目录的骨架
  - **仓库中不提交完整源码内容**
  - 这里只保留 README 和占位目录，方便说明环境准备流程

- `linux-driver-lab/`
  - 项目主体
  - 包括每一天的驱动代码、测试程序、脚本、README 和复盘文档

---

## 推荐阅读顺序

第一次使用本仓库，建议按下面顺序阅读。

1. `kernel-src/README.md`
2. `kernel-src/linux-5.15.10/README.md`
3. `kernel-src/busybox-1.36.1/README.md`
4. `linux-driver-lab/README.md`
5. `linux-driver-lab/day01 ~ day07`

这样可以先把环境准备清楚，再看代码和实验内容。

---

## 环境准备入口

### 1. Windows 宿主机

如果你是在 Windows 上通过 VMware 安装 Ubuntu，再在 Ubuntu 中运行 QEMU 做实验，建议先看：

- `kernel-src/README.md`

其中包含：

- Hyper-V / 内核隔离 / 内存完整性 导致虚拟化被占用的问题
- VMware 处理器页面中“虚拟化引擎”勾选说明
- 为什么 Ubuntu 虚拟机里再跑 QEMU 属于嵌套虚拟化
- 为什么不勾选虚拟化支持会导致 QEMU 极慢

### 2. Ubuntu 虚拟机

环境准备说明在：

- `kernel-src/linux-5.15.10/README.md`
- `kernel-src/busybox-1.36.1/README.md`

其中包含：

- Ubuntu 依赖安装
- Linux 5.15.10 下载与编译
- `make menuconfig` 检查模块支持
- BusyBox 1.36.1 下载与编译
- 如何验证环境是否准备完成

---

## 为什么保留 `kernel-src/` 骨架

本仓库依赖：

- Linux 内核源码
- BusyBox 源码与安装结果

但这些源码本身通常不适合直接提交进 GitHub 仓库，所以这里采用折中方案：

- **保留目录结构**
- **保留说明文档**
- **不提交完整源码内容**

这样做的好处是：

- 仓库结构清晰
- 环境依赖明确
- GitHub 仓库不会过大
- 别人 clone 后知道该把源码放到哪里

---

## 快速开始

### 第一步：准备环境

先按文档准备这两个目录：

```text
kernel-src/linux-5.15.10/
kernel-src/busybox-1.36.1/
```

详细步骤见：

- `kernel-src/linux-5.15.10/README.md`
- `kernel-src/busybox-1.36.1/README.md`

---

### 第二步：进入实验项目

```bash
cd linux-driver-lab
```

建议先看：

- `linux-driver-lab/README.md`

---

### 第三步：构建 day06 实验

```bash
cd day06
chmod +x build.sh
./build.sh
```

当前脚本的路径规则是：

- 优先使用仓库内相对路径
- 找不到时再兼容旧的绝对路径回退

也就是说，优先查找：

```text
../kernel-src/linux-5.15.10
../kernel-src/busybox-1.36.1
```

如果你的目录结构不是这种形式，也可以手工指定：

```bash
KDIR=/path/to/linux-5.15.10 BUSYBOX_DIR=/path/to/busybox-1.36.1 ./build.sh
```

---

### 第四步：进入 QEMU 后执行

```sh
/bin/all.sh
```

或者分步执行：

```sh
/bin/insmod_rmmod.sh 500
insmod /demo.ko
/bin/stress_rw.sh 300
/bin/check_dmesg.sh
```

---

## Day01 ~ Day07 概览

### day01
- 基础字符设备驱动框架
- 模块加载、设备节点、open/release

### day02
- 读写接口雏形
- 用户态测试程序初步联动

### day03
- `ioctl` 接口加入
- 数据通路开始完整

### day04
- 阻塞读模型引入
- waitqueue 初步使用

### day05
- workqueue 引入
- 形成“提交任务 -> 异步处理 -> 读结果”的完整链路

### day06
- 回归测试脚本
- 压力测试脚本
- `insmod/rmmod 500 次`
- 并发读写 300 秒
- `dmesg` 异常扫描
- 已完成正式验收

### day07
- README 收口整理
- 一页复盘输出
- 环境文档补齐
- GitHub 结构整理

---

## 当前推荐的学习重点

如果你是初学者，当前最值得重点理解的是 day05 和 day06。

### waitqueue

用于条件等待和阻塞唤醒。

一句话理解：

> 条件不满足时，先睡；条件满足后，再唤醒继续执行。

常见场景：

- 阻塞式 `read()`
- 等待数据到达
- 等待异步处理完成

### workqueue

用于把任务放到内核工作线程中异步处理。

一句话理解：

> 这个活先别在当前路径做，放到后台线程里做。

常见场景：

- 异步任务处理
- 中断下半部
- 可能睡眠的后续操作

### 两者如何配合

在当前实验驱动中：

1. 用户通过 `write()` 或 `ioctl()` 提交输入
2. 驱动把处理逻辑放进 `workqueue`
3. `read()` 若结果未准备好，则通过 `waitqueue` 阻塞等待
4. work 完成后设置结果并唤醒读者
5. 用户从 `read()` 中取走结果

### 餐馆类比

- `write()` / `ioctl()`：顾客下单
- `workqueue`：后厨做菜
- `waitqueue`：顾客等叫号
- `read()`：顾客取餐

所以：

- `workqueue` 是“做事的人”
- `waitqueue` 是“等结果的人”

---

## Day06 当前验收结论

当前项目已经完成 day06 正式验收，目标包括：

- `insmod/rmmod 500 次`
- 并发读写 `300s`
- 脚本通过
- 无 Oops
- `dmesg` 无可疑泄漏告警

同时已经修复了早期版本中 reader 在阻塞 `read()` 场景下脚本可能无法正常回收的问题。

---

## 文档入口

### 项目主文档
- `linux-driver-lab/README.md`

### 环境准备
- `kernel-src/README.md`
- `kernel-src/linux-5.15.10/README.md`
- `kernel-src/busybox-1.36.1/README.md`

### 过程记录与复盘
- `linux-driver-lab/docs/PROGRESS.md`
- `linux-driver-lab/docs/W1_REVIEW.md`

### Day07 收口说明
- `linux-driver-lab/day07/README.md`

---

## GitHub 提交说明

本仓库推荐提交：

- `kernel-src/` 的目录骨架和 README
- `linux-driver-lab/` 下的代码、脚本和文档

本仓库不建议提交：

- 完整 Linux 内核源码树
- 完整 BusyBox 源码树
- `*.ko`
- `*.o`
- `bzImage`
- `rootfs.cpio.gz`
- 运行日志、压缩包等构建产物

---

## 适合谁看

这个仓库适合：

- Linux 驱动入门学习者
- 想练习字符设备驱动的人
- 想理解 waitqueue / workqueue 的人
- 想搭一套可复现的 QEMU 驱动实验环境的人
- 想把学习过程整理成 GitHub 项目的人

---

## License

本仓库采用 Apache License 2.0。

详见根目录 `LICENSE`。
