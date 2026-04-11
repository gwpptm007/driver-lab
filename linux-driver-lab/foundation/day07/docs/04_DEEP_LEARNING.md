# Day07 深度指南 - W1 收口与项目整理

## 一、Day07 是什么？

Day07 是 W1（字符设备基础）的最后一天，定位是**W1 文档收口 + 项目可迁移化**。

**核心目标**：把 day01-06 的经验整理成可迁移文档，让"别人拿到仓库后能照着搭环境并跑通"，同时沉淀一份 W1 阶段复盘。

Day07 不新增驱动功能。它的重点是：
1. **README 完整化**：仓库总览、kernel-src/ 目录骨架、Windows + VMware + Ubuntu + QEMU 关系
2. **build.sh 可迁移化**：相对路径优先，兼容历史旧路径，支持 KDIR/BUSYBOX_DIR 环境变量
3. **W1 一页复盘**：从接口、设计、风险、回归四个维度回顾 W1
4. **环境准备文档化**：kernel-src/README.md 说明如何补齐内核和 BusyBox

---

## 二、W1 学习路径中的位置

### 2.1 W1 整体架构

```
W1 (字符设备基础 - day01-07)
├── day01: miscdevice 字符设备入口
├── day02: ioctl 命令定义
├── day03: 等待队列 waitqueue 阻塞读
├── day04: 工作队列 workqueue 异步处理
├── day05: sysfs / debugfs 可观测性
├── day06: 回归脚本 + 压力测试
└── day07: W1 收口 + 环境整理        ← 今天
```

### 2.2 Day07 与前后天的关系

```
Day06 vs Day07：
  - Day06：验证代码是否"稳"
  - Day07：验证文档是否"完整"

Day07 vs Day08：
  - Day07：W1 收口
  - Day08：W2 开启（platform_driver + DT）
```

---

## 三、Day07 做了什么

### 3.1 README 完整化

```
新增/完善的文档：
  - linux-driver-lab/README.md：仓库总览 + 学习路径
  - kernel-src/README.md：内核/BusyBox 环境准备
  - docs/ROADMAP.md：周学习目标
  - docs/PROGRESS.md：进度追踪

关键信息：
  - kernel-src/ 目录骨架的意义
  - Windows + VMware + Ubuntu + QEMU 的实验链路
  - 依赖安装命令
```

### 3.2 build.sh 可迁移化

```
旧问题：
  - build.sh 里的路径硬编码为 /home/wq7/workspace/kernel-src/...
  - 仓库换到别的目录就全挂了

新设计：
  - 优先使用仓库相对路径
  - 兼容历史 /home/wq7/workspace/... 旧路径
  - 支持环境变量 KDIR / BUSYBOX_DIR 覆盖

好处：
  - 仓库 clone 到任何路径都能用
  - 开发者可以指定自己的内核/BusyBox 位置
```

### 3.3 W1 一页复盘（docs/W1_REVIEW.md）

```
从四个维度回顾 W1：

接口维度：
  - /dev/demo 节点怎么出现（alloc_chrdev_region → cdev_add → class_create → device_create）
  - ioctl 命令怎么定义（_IOW/_IOR/_IOWR）
  - sysfs / debugfs 怎么导出

设计维度：
  - waitqueue 做阻塞读
  - workqueue 做异步处理
  - mutex 做并发保护
  - 单槽 pending work 模型

风险维度：
  - cancel_work_sync() 防 UAF
  - 单槽模型下 -EBUSY 是预期行为
  - 并发压测允许 busy 非 0，但 err 必须为 0

回归维度：
  - insmod/rmmod 500 次验证 init/remove 干净
  - stress_rw.sh 验证竞态安全
  - check_dmesg.sh 捕获内核告警
```

---

## 四、kernel-src/ 目录骨架

### 4.1 为什么要保留骨架但不提交源码？

```
kernel-src/ 是教学环境的依赖目录：

kernel-src/
├── linux-5.15.10/
│   ├── src/           ← 内核源码（需要用户自行解压）
│   ├── build/          ← 内核编译产物
│   └── output/         ← 内核 Image 输出
└── busybox-1.36.1/
    ├── src/           ← BusyBox 源码（需要用户自行解压）
    ├── build/          ← BusyBox 编译产物
    └── output/         ← BusyBox 安装产物

理由：
  - 内核/BusyBox 源码体积大（数百 MB ~ 数 GB）
  - 不适合提交到 Git 仓库
  - 但目录骨架和准备脚本保留下来，能指导用户快速补齐
```

### 4.2 用户拿到仓库后怎么补齐？

```
1. 克隆仓库
2. 解压内核源码到 kernel-src/linux-5.15.10/src
3. 编译内核（参考 kernel-src/README.md）
4. 解压 BusyBox 源码到 kernel-src/busybox-1.36.1/src
5. 编译 BusyBox（静态链接）
6. 进入任意 day 目录，执行 ./build.sh
```

---

## 五、build.sh 路径规则

### 5.1 三层路径解析

```
第一层：环境变量优先
  KDIR=/path/to/kernel
  BUSYBOX_DIR=/path/to/busybox
  → 直接使用用户指定路径

第二层：仓库相对路径
  $SCRIPT_DIR/../kernel-src/linux-5.15.10/...
  → 相对于仓库根目录的位置

第三层：历史旧路径兼容
  /home/wq7/workspace/kernel-src/...
  → 如果存在就使用（避免老用户环境失效）
```

### 5.2 为什么这样设计？

```
目标：让仓库在任何路径下都能用

- 不同开发者的工作目录不同
- 换了电脑后路径也不同
- 但代码应该是一样的

环境变量 > 相对路径 > 历史旧路径
= 最大灵活性 + 最小破坏性
```

---

## 六、W1 核心能力总结

### 6.1 接口层（W1 学到了什么）

```
字符设备注册链路：
  alloc_chrdev_region() → cdev_add() → class_create() → device_create()
  → /dev/demo 自动出现

ioctl 命令定义：
  _IOW(DEMO_IOC_MAGIC, nr, type)
  _IOR(DEMO_IOC_MAGIC, nr, type)
  _IOWR(DEMO_IOC_MAGIC, nr, type)

waitqueue + workqueue：
  write/ioctl(SET) → schedule_work() → worker 异步处理 → wake_up_interruptible()
  read() → wait_event_interruptible() 阻塞 → 被唤醒后 copy_to_user()

sysfs / debugfs：
  sysfs：enable/counter 等属性导出
  debugfs：status/log_level 等调试信息
```

### 6.2 设计层（W1 学到了什么）

```
并发保护：
  - mutex：保护共享状态（read/write/ioctl/work_handler 都用）
  - 不能在 atomic context（硬中断）用 mutex

异步处理：
  - workqueue：把慢处理下沉到进程上下文
  - schedule_work() 只负责"排队"，不等待完成

阻塞读：
  - wait_event_interruptible()：可被信号打断的睡眠
  - wake_up_interruptible()：唤醒对应进程
  - 醒来后二次检查（spurious wakeup 防护）
```

### 6.3 风险层（W1 学到了什么）

```
UAF 防护：
  - cancel_work_sync() 在卸载前等待 worker 退出

单槽模型：
  - work_pending：同一时刻只允许一个 work
  - -EBUSY 不是 bug，是设计决定的

压测通过标准：
  - err = 0（真正的失败）
  - busy/timeout 允许非 0（预期行为）
```

### 6.4 回归层（W1 学到了什么）

```
500 次装卸回归：init/remove 路径
5 分钟并发压测：竞态安全
dmesg 扫描：Oops/UAF/leak 捕获

all.sh 一键走完 = Day06 验收通过
```

---

## 七、Day07 与 Day08 的关系

### 7.1 W1 → W2 的切换

```
W1（day01-07）：字符设备基础
  核心问题：怎么给用户态提供 /dev 接口
  重点：cdev / waitqueue / workqueue / sysfs / debugfs

W2（day08-14）：嵌入式驱动模式
  核心问题：驱动怎么绑定设备、怎么管理资源
  重点：platform_driver / Device Tree / IRQ / regmap / ftrace

Day07 做 W1 收口，Day08 切换到 W2
```

### 7.2 Day07 的验收方式

```
不是"功能验收"，而是"文档验收"：

- 别人 clone 仓库后能照着 README 补齐环境
- 进入 day06 执行 ./build.sh 能跑通
- /bin/all.sh 在 guest 里能一键验收

如果能做到这两点，说明 Day07 的文档收口有效
```

---

## 八、面试要会讲的五句话

1. **"Day07 是 W1 的最后一天，重点不是新增代码功能，而是把 day01-06 的经验整理成可迁移文档：README 完整化、build.sh 可迁移化、W1 一页复盘"**
   → 理解 Day07 的定位

2. **"kernel-src/ 目录骨架不提交完整源码（体积太大），但保留了目录结构和准备脚本，指导用户快速补齐内核和 BusyBox 环境"**
   → 理解 kernel-src/ 的设计

3. **"build.sh 使用'环境变量 > 相对路径 > 历史旧路径'三层解析逻辑，让仓库在任何路径下都能用，同时兼容老用户的历史路径"**
   → 理解 build.sh 可迁移化设计

4. **"W1 一页复盘从接口、设计、风险、回归四个维度总结：接口层学到 cdev + ioctl + waitqueue + workqueue + sysfs/debugfs，设计层学到 mutex 并发保护和单槽模型，风险层学到 cancel_work_sync 防 UAF，回归层学到 insmod_rmmod + stress_rw + check_dmesg 三件套"**
   → 理解 W1 完整收获

5. **"Day07 验收的标准不是'功能'，而是'别人拿到仓库后能照着文档跑通'——这是工程交付质量的一部分"**
   → 理解 Day07 验收的特殊性

---

## 九、验收标准

### 9.1 文档完整性

- [ ] linux-driver-lab/README.md 包含仓库总览 + 学习路径
- [ ] kernel-src/README.md 说明如何准备环境
- [ ] docs/W1_REVIEW.md 存在并包含四个维度的复盘

### 9.2 build.sh 可迁移性

- [ ] 进入 day06，执行 `chmod +x build.sh && ./build.sh` 能正常启动 QEMU
- [ ] 环境变量 KDIR / BUSYBOX_DIR 能正确覆盖默认路径

### 9.3 W1 一键验收

- [ ] /bin/all.sh 在 guest 里能走完 W1 验收流程
- [ ] 无 Oops/UAF/leak 告警
