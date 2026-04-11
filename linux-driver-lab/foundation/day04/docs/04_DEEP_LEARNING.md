# Day04 深度指南 - debugfs 调试快照与可观测性

## 一、Day04 是什么？

Day04 是 W1（字符设备基础）的第四天，定位是**debugfs 调试接口 + 驱动的可观测性建设**。

**核心目标**：在 Day03 sysfs 控制面的基础上，叠加 debugfs 调试面——用 status 快照看内部状态，用 log_level 动态控制日志开关，形成完整的"控制面 + 功能面 + 观测面"三维结构。

Day04 不做 waitqueue，不做 workqueue。它的重点是：
1. **debugfs_create_dir**：在 /sys/kernel/debug/ 下创建调试目录
2. **debugfs_create_file**：导出 status 快照文件
3. **debugfs_create_u32**：导出 log_level 可调参数
4. **simple_read_from_buffer**：seq_file 风格的调试读取
5. **pr_info_ratelimited**：限速日志防止刷屏

---

## 二、W1 学习路径中的位置

### 2.1 W1 整体架构

```
W1 (字符设备基础 - day01-07)
├── day01: miscdevice 字符设备入口
├── day02: ioctl 命令定义
├── day03: sysfs 属性接口
├── day04: debugfs 调试快照      ← 今天
├── day05: waitqueue + workqueue
├── day06: 回归脚本 + 压力测试
└── day07: W1 收口 + 环境整理
```

### 2.2 Day04 与前后天的关系

```
Day03 vs Day04：
  - Day03：sysfs 做控制面（enable）和状态面（counter）
  - Day04：增加 debugfs 做调试面（status/log_level）

Day04 vs Day05：
  - Day04：建立调试观测的基础结构（debugfs + struct demo_device）
  - Day05：在同结构上叠加 waitqueue + workqueue
  - Day05 复用 Day04 的 struct demo_device、debugfs、sysfs

Day04 是 Day05 的"观测基础设施"
```

---

## 三、为什么需要 debugfs？

### 3.1 sysfs vs debugfs 的定位

```
sysfs（/sys）：
  - 面向运维和应用的稳定接口
  - 强调长期 ABI 兼容性
  - 适合导出配置项、状态项
  - 例如：enable、counter、功率、频率

debugfs（/sys/kernel/debug/）：
  - 面向开发和调试的临时接口
  - 不保证长期 ABI 稳定
  - 适合导出内部快照、调试统计、临时开关
  - 例如：status、log_level、内部寄存器快照

简单记忆：
  sysfs = 正式面（给运维/应用）
  debugfs = 调试面（给开发者）
```

### 3.2 Day04 的 debugfs 结构

```
/sys/kernel/debug/demo_debug/
       │
       ├── status (0444)
       │     └── cat status → 打印内部状态快照
       │
       └── log_level (0644)
             ├── cat log_level → 读当前值
             └── echo N > log_level → 改当前值
```

---

## 四、debugfs API 详解

### 4.1 debugfs_create_dir

```c
// 创建 /sys/kernel/debug/demo_debug 目录
g_demo_dev->debug_root = debugfs_create_dir("demo_debug", NULL);
// 第二个参数 NULL 表示在 /sys/kernel/debug/ 下创建
// 如果不是 NULL，就在那个 dentry 下创建子目录
```

```
debugfs_create_dir 的特点：
  - 自动创建父目录（如果不存在）
  - 失败返回 ERR_PTR 或 NULL
  - 不需要手动创建多层目录
```

### 4.2 debugfs_create_file

```c
// 创建 /sys/kernel/debug/demo_debug/status
debugfs_create_file("status",              // 文件名
                    0444,                  // 权限（只读）
                    g_demo_dev->debug_root, // 父目录
                    NULL,                  // 传递给回调的私有数据
                    &debug_status_fops);   // 文件操作回调
```

### 4.3 debugfs_create_u32

```c
// 直接绑定一个 u32 变量，不需要 show/store 回调
debugfs_create_u32("log_level",            // 文件名
                   0644,                   // 权限（可读写）
                   g_demo_dev->debug_root,  // 父目录
                   &g_demo_dev->log_level); // 关联的内核变量
```

```
debugfs_create_u32 的便利：
  - 内核自动处理读写
  - 不需要写 show/store 回调
  - 变量直接暴露给用户态

但局限：
  - 只能是 u32 类型
  - 不能做复杂解析/格式化
```

### 4.4 debugfs 文件操作的两种方式

```
方式一：直接绑定变量（debugfs_create_u32）
  - 自动处理读写
  - 适合简单开关、数值参数

方式二：自定义文件操作（debugfs_create_file）
  - 需要定义 struct file_operations
  - 适合复杂格式化输出
  - 适合需要组合多个字段的场景
```

---

## 五、Day04 驱动代码分析

### 5.1 struct demo_device 的完整封装

```c
struct demo_device {
    struct cdev cdev;           // 字符设备
    struct class *class;        // sysfs 类
    struct device *device;       // sysfs 设备
    dev_t dev_id;               // 设备号

    int enable;                 // Day03：开关
    int counter;                // Day03：计数器
    u32 log_level;              // Day04：日志级别

    struct dentry *debug_root;  // Day04：debugfs 根目录
};
```

### 5.2 status 快照的实现

```c
static ssize_t demo_status_read(struct file *file,
                                char __user *user_buf,
                                size_t count, loff_t *ppos)
{
    char buf[512];
    int len;

    len = snprintf(buf, sizeof(buf),
        "---------- Day 04 Snapshot ----------\n"
        "Device Enable : %s\n"
        "IOCTL Count   : %d\n"
        "Debug Log Lvl : %u\n"
        "Kernel Time   : %lu s\n"
        "-------------------------------------\n",
        g_demo_dev->enable ? "ON" : "OFF",
        g_demo_dev->counter,
        g_demo_dev->log_level,
        (unsigned long)(jiffies / HZ));

    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations debug_status_fops = {
    .owner = THIS_MODULE,
    .read  = demo_status_read,
};
```

```
demo_status_read 的要点：
  1. 格式化到内核栈缓冲区 buf
  2. 用 simple_read_from_buffer 拷贝到用户态
  3. 返回实际写入的字节数

为什么用 simple_read_from_buffer？
  - 它处理了 ppos（偏移）逻辑
  - 支持部分读（count < len 的情况）
  - 比直接 copy_to_user 更安全

jiffies / HZ 的含义：
  - jiffies：系统启动以来的时钟节拍数
  - HZ：每秒节拍数（CONFIG_HZ，通常 100/250/300/1000）
  - jiffies / HZ = 启动秒数
```

### 5.3 pr_info_ratelimited 的使用

```c
if (g_demo_dev->log_level > 0)
    pr_info_ratelimited("Demo: IOCTL handled (Count: %d)\n",
                        g_demo_dev->counter);
```

```
pr_info vs pr_info_ratelimited：

pr_info：
  - 每次都打印
  - 高频路径（如中断）可能刷爆控制台

pr_info_ratelimited：
  - 有速率限制，防止刷屏
  - 通过 token bucket 算法实现
  - 适合"调试时要看到，但又不想刷屏"的场景

log_level 的作用：
  - log_level = 0：完全关闭日志
  - log_level > 0：打开日志
  - 通过 debugfs 直接改，立即生效
```

---

## 六、Day04 vs Day05 的关系

### 6.1 Day04 为 Day05 打基础

```
Day04 建立的（Day05 继续用）：
  1. struct demo_device 封装
  2. debugfs status/log_level
  3. sysfs enable/counter
  4. cdev + class + device 注册模式

Day05 新增：
  1. waitqueue（阻塞读）
  2. workqueue（异步处理）
  3. mutex（并发保护）
  4. cancel_work_sync（卸载前同步）

Day04 和 Day05 是在同一个结构上的功能叠加
```

### 6.2 Day04 的 ioctl 为 Day05 留的接口

```c
if (!g_demo_dev->enable) {
    if (g_demo_dev->log_level > 0)
        pr_info_ratelimited("Demo: Device disabled, IOCTL rejected!\n");
    return -EPERM;
}
```

```
Day04 的 enable 检查：
  - Day05 的 waitqueue 读路径也会检查 enable
  - 如果 enable=0，read() 会返回 -EPERM

Day05 的单槽 pending work 模型：
  - Day04 的 counter 没有并发保护
  - Day05 会用 mutex 保护所有共享状态
```

---

## 七、面试要会讲的五句话

1. **"debugfs 是 Linux 的调试文件系统，挂载在 /sys/kernel/debug/，用于导出调试快照和临时参数；sysfs 强调稳定 ABI 兼容（给运维用），debugfs 强调临时调试（给开发用），两者定位不同但可以共存"**
   → 理解 sysfs vs debugfs 的定位

2. **"debugfs_create_dir 创建调试目录，debugfs_create_file 创建自定义文件操作节点，debugfs_create_u32 直接绑定 u32 变量不需要回调；Day04 的 status 用 demo_status_read 格式化快照，log_level 用 debugfs_create_u32 直接绑定变量"**
   → 理解 debugfs 三种创建方式的区别

3. **"pr_info_ratelimited() 是限速日志，防止高频路径刷爆控制台；log_level 通过 debugfs 直接暴露给用户，echo 0 > log_level 可以完全关闭日志，echo 1 > 打开，这是驱动调试时的常用模式"**
   → 理解限速日志和动态日志开关

4. **"simple_read_from_buffer() 是 seq_file 风格的简化读取接口，自动处理偏移量 ppos 和部分读，适合调试输出；status 快照把多个内部字段格式化成文本，一次性返回给用户"**
   → 理解调试快照的实现方式

5. **"Day04 和 Day05 的关系是骨架不变、功能叠加：Day04 建立了 struct demo_device + sysfs + debugfs 的基础，Day05 在同结构上增加 waitqueue + workqueue + mutex；这演示了真实驱动开发中'先搭结构、再叠功能'的模式"**
   → 理解 Day04 → Day05 的演进

---

## 八、验收标准

### 8.1 debugfs 结构验收

- [ ] `ls /sys/kernel/debug/demo_debug/` 存在
- [ ] `status` 和 `log_level` 文件存在

### 8.2 status 快照验收

- [ ] `cat /sys/kernel/debug/demo_debug/status` 输出格式化状态
- [ ] 包含 enable、counter、log_level、时间戳

### 8.3 log_level 动态控制验收

- [ ] `cat /sys/kernel/debug/demo_debug/log_level` 返回默认值
- [ ] `echo 0 > /sys/kernel/debug/demo_debug/log_level` 关闭日志
- [ ] `echo 1 > /sys/kernel/debug/demo_debug/log_level` 打开日志

### 8.4 enable 控制验收

- [ ] `echo 0 > /sys/class/demo_day04/demo_day04/enable` 后 ioctl 被拒绝
- [ ] `dmesg | tail` 显示 `Device disabled, IOCTL rejected!`

---

## 附录：Day04 完整文件操作结构图

```
/sys/kernel/debug/demo_debug/
       │
       ├── status (0444, 只读)
       │     └── read
       │          demo_status_read()
       │          → snprintf 格式化到 buf
       │          → simple_read_from_buffer → 用户态
       │
       └── log_level (0644, 可读写)
             ├── read  → 直接返回 u32 值
             └── write → 直接写入 u32 变量

/sys/class/demo_day04/demo_day04/
       │
       ├── enable (0644, 可读写)
       │     ├── read  → enable_show()
       │     └── write → enable_store()
       │
       └── counter (0444, 只读)
             └── read → counter_show()
```
