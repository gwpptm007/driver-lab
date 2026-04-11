# Day03 深度指南 - sysfs 属性接口与驱动的控制面/状态面

## 一、Day03 是什么？

Day03 是 W1（字符设备基础）的第三天，定位是**sysfs 属性导出 + 设备对象封装**。

**核心目标**：在 Day02 的 ioctl 功能面上，叠加 sysfs 控制面（enable 开关）和状态面（counter 统计），让驱动真正具备"可控制、可观测"的能力。

Day03 不做 waitqueue，不做 workqueue。它的重点是：
1. **class_create/device_create**：在 /sys/class/ 下创建设备类
2. **DEVICE_ATTR_RW/RO**：声明 sysfs 属性
3. **enable_show/enable_store**：sysfs 读写回调
4. **struct demo_device**：设备对象封装
5. **enable 控制 + counter 统计 + ioctl 功能的三角关系**

---

## 二、W1 学习路径中的位置

### 2.1 W1 整体架构

```
W1 (字符设备基础 - day01-07)
├── day01: miscdevice 字符设备入口
├── day02: ioctl 命令定义
├── day03: sysfs 属性接口     ← 今天
├── day04: debugfs 状态快照
├── day05: waitqueue + workqueue
├── day06: 回归脚本 + 压力测试
└── day07: W1 收口 + 环境整理
```

### 2.2 Day03 与前后天的关系

```
Day02 vs Day03：
  - Day02：ioctl 做 SET/GET，但状态是散放的全局变量
  - Day03：增加 sysfs 属性（enable/counter），设备封装到结构体

Day03 vs Day04：
  - Day03：sysfs 做控制面和状态导出
  - Day04：debugfs 做调试快照和日志开关

Day03 + Day04 vs Day05：
  - Day03/Day04 的基础结构被 Day05 复用
  - Day05 在此基础上增加 waitqueue + workqueue
```

---

## 三、为什么需要 sysfs？

### 3.1 sysfs 的定位

```
Linux 设备模型的核心：
  /sys
    ├── block/          块设备
    ├── bus/            总线（platform, pci, usb...）
    ├── class/          设备类（demo, net, input...）
    ├── devices/        设备树
    ├── drivers/        驱动
    └── ...

sysfs 的本质：
  把内核里的设备对象、属性、层次关系导出到用户空间

驱动最常用的场景：
  - /sys/class/xxx/yyy：设备类下的属性文件
  - cat xxx：读取状态
  - echo val > xxx：写入配置
```

### 3.2 sysfs vs ioctl 的分工

```
sysfs：适合简单的"读状态/写配置"
  - 通过文件接口（cat/echo）操作
  - 不需要写专门的测试程序
  - 适合运维和快速调试

ioctl：适合复杂的"命令/数据交互"
  - 需要专门的测试程序
  - 适合传递结构化数据
  - 适合需要返回值和错误码的场景

Day03 的设计：
  sysfs enable     → 控制设备开关（运维视角）
  sysfs counter    → 统计操作次数（观测视角）
  ioctl SET/GET   → 设备值交互（功能视角）
```

---

## 四、sysfs 属性导出详解

### 4.1 Day03 的 sysfs 结构

```
/sys/class/demo/demo/
├── enable          ← 可读写：echo 0/1 > enable
├── counter        ← 只读：cat counter
├── dev            ← 设备号：248:0
├── power/         ← 电源管理（内核自动创建）
└── uevent         ← 热插拔事件（内核自动创建）
```

### 4.2 class_create + device_create

```c
// 在 /sys/class/ 下创建 demo 目录
demo_class = class_create(THIS_MODULE, "demo");

// 在 /sys/class/demo/ 下创建 demo 设备
demo_device = device_create(demo_class, NULL,
                            MKDEV(major, 0), NULL,
                            "demo");

// 结果：/sys/class/demo/demo/ 自动出现
```

```
class_create() 的变化（Linux 5.x vs 旧版本）：

旧版本（~3.x）：
  struct class *class_create(struct module *owner, const char *name)

新版本（5.x+）：
  struct class *class_create(const char *name)
  （owner 参数被移除）

Day03 用的是新版本（5.x）！
```

### 4.3 DEVICE_ATTR_RW/RO 宏

```c
// 声明 enable 属性：可读可写
static DEVICE_ATTR_RW(enable);

// 声明 counter 属性：只读
static DEVICE_ATTR_RO(counter);
```

```
DEVICE_ATTR_RO(name) 展开：
  static struct device_attribute dev_attr_##name = {
      .attr = { .name = #name, .mode = 0444 },
      .show = name##_show,
      .store = NULL,
  }

DEVICE_ATTR_RW(name) 展开：
  static struct device_attribute dev_attr_##name = {
      .attr = { .name = #name, .mode = 0644 },
      .show = name##_show,
      .store = name##_store,
  }

0644 的含义：
  - 所有者：rw-
  - 组：r--
  - 其他：r--
```

### 4.4 show/store 回调的实现

```c
static ssize_t enable_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", demo_enable);
}

static ssize_t enable_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
    int val;
    if (kstrtoint(buf, 10, &val) == 0) {
        demo_enable = (val > 0) ? 1 : 0;
        pr_info("Demo: Enable set to %d\n", demo_enable);
    }
    return count;
}
```

```
用户执行：echo 1 > /sys/class/demo/demo/enable

1. 内核解析 "1" 为整数
2. 调用 enable_store(dev, attr, "1", 2)
3. kstrtoint("1", 10, &val) → val = 1
4. demo_enable = 1
5. 返回 count（表示接受了 2 个字符）

为什么用 kstrtoint 而不是 simple_strtol？
  - kstrtoint：专门的字符串转整数接口，错误处理清晰
  - 返回 0 表示成功，非零表示失败
```

---

## 五、设备对象封装

### 5.1 Day03 的全局变量

```c
static int major;
static int demo_enable = 1;   // 开关变量
static int demo_counter = 0;  // 计数器变量

static struct class *demo_class;
static struct device *demo_device;
```

```
Day03 vs Day02：
  Day02：kernel_value 是全局标量
  Day03：引入了 class 和 device 指针

但 Day03 仍然用全局变量而不是 struct demo_device，
这是因为 Day03 是过渡阶段。

Day04 会正式引入 struct demo_device 封装。
```

### 5.2 ioctl 的 enable 检查

```c
static long demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (!demo_enable) {
        pr_warn("Demo: Device is disabled! Operation rejected.\n");
        return -EPERM;
    }
    ...
}
```

```
enable 的控制逻辑：
  - echo 0 > enable → demo_enable = 0 → ioctl 全部返回 -EPERM
  - echo 1 > enable → demo_enable = 1 → ioctl 恢复正常

这演示了 sysfs 控制面影响 ioctl 功能面的典型模式。
```

---

## 六、Day03 vs Day04 的关系

### 6.1 Day03 的局限

```
Day03 的问题：
  1. counter 是全局 int，没有锁保护（Day05 会有并发问题）
  2. 没有 debugfs 导出调试信息
  3. counter 只计数，不区分命令类型
  4. 设备对象还没封装

Day04 的改进：
  1. 引入 struct demo_device 封装所有状态
  2. 增加 debugfs（status/log_level）
  3. counter 仍然没有锁（留给 Day05）
  4. 过渡到更工程化的结构
```

### 6.2 Day03 → Day04 的代码演进

```
Day03：
  static int demo_enable = 1;
  static int demo_counter = 0;

Day04：
  struct demo_device {
      int enable;
      int counter;
      u32 log_level;
      struct dentry *debug_root;
      struct cdev cdev;
      struct class *class;
      struct device *device;
      dev_t dev_id;
  };
  static struct demo_device *g_demo_dev;

演进原因：
  - 全局变量在多设备场景下无法扩展
  - 结构体封装后，状态和操作都更清晰
  - 符合 Linux 设备模型的思维方式
```

---

## 七、面试要会讲的五句话

1. **"sysfs 是 Linux 的虚拟文件系统，挂载在 /sys，用于导出内核设备对象的属性；class_create/device_create 在 /sys/class/ 下创建设备类，DEVICE_ATTR_RW/RO 声明属性文件的 show/store 回调，cat/echo 操作触发对应回调"**
   → 理解 sysfs 的定位和基本 API

2. **"sysfs 适合简单的读状态/写配置（cat/echo），ioctl 适合复杂的命令/数据交互（需要测试程序）；Day03 的设计是 sysfs enable 做控制面、sysfs counter 做状态面、ioctl 做功能面，三者配合"**
   → 理解 sysfs vs ioctl 的分工

3. **"DEVICE_ATTR_RW(name) 展开后包含 .show = name##_show, .store = name##_store, .mode = 0644；enable_store 用 kstrtoint 解析用户输入而不是简单 strtol，因为 kstrtoint 有清晰的错误返回值"**
   → 理解 DEVICE_ATTR 宏的展开

4. **"Day03 和 Day04 的关系是：Day03 用全局变量，Day04 引入 struct demo_device 封装所有状态；这是从'教学演示'到'工程化'的过渡，sysfs/debugfs 的导出模式不变，只是承载状态的数据结构变了"**
   → 理解 Day03 → Day04 的演进

5. **"sysfs 的 show 回调返回 sprintf(buf, '%d\n', value)，必须加换行符否则 cat 输出格式不对；store 回调返回 count 表示接受了多少输入，返回错误码表示拒绝"**
   → 理解 sysfs 回调的返回值语义

---

## 八、验收标准

### 8.1 sysfs 结构验收

- [ ] `ls /sys/class/demo/demo/` 存在
- [ ] `enable` 和 `counter` 属性存在

### 8.2 enable 控制验收

- [ ] `cat /sys/class/demo/demo/enable` 返回 `1`
- [ ] `echo 0 > /sys/class/demo/demo/enable` 后 ioctl 返回 `-EPERM`
- [ ] `echo 1 > /sys/class/demo/demo/enable` 后功能恢复

### 8.3 counter 统计验收

- [ ] 初始 `cat /sys/class/demo/demo/counter` 为 0
- [ ] 每次 SET+GET 操作后 counter 增加 2

### 8.4 dmesg 验收

- [ ] `dmesg | grep "Demo:"` 显示 SET/GET 日志
- [ ] `dmesg | grep "Enable set to"` 显示 enable 变化

---

## 附录：Day03 的 sysfs 属性完整图解

```
/sys/class/demo/demo/
       │
       ├── enable (rw)
       │     ├── read  → enable_show()  → sprintf(buf, "%d\n", demo_enable)
       │     └── write → enable_store() → kstrtoint() → demo_enable = val
       │
       ├── counter (ro)
       │     └── read  → counter_show() → sprintf(buf, "%d\n", demo_counter)
       │
       ├── dev (ro, 内核自动创建)
       │     └── 248:0 格式的设备号
       │
       ├── power/ (内核自动创建)
       │
       └── uevent (内核自动创建)
             └── 热插拔事件文件
```
