# Day02 深度指南 - ioctl 命令定义与用户态/内核态数据交互

## 一、Day02 是什么？

Day02 是 W1（字符设备基础）的第二天，定位是**ioctl 协议设计 + ABI 数据搬运**。

**核心目标**：在 Day01 跑通的"驱动即文件"骨架上，建立"驱动与用户程序如何对话"的协议思维——命令怎么定义、数据怎么搬运、错误码怎么返回。

Day02 不做 sysfs，不做 waitqueue。它的重点是：
1. **ioctl 命令号设计**：`DEMO_IOC_MAGIC`、`_IOW`/`_IOR` 宏
2. **数据搬运**：`copy_from_user()` 和 `copy_to_user()` 的使用
3. **错误码语义**：`0`=成功、`-EFAULT`=地址错误、`-EINVAL`=参数无效
4. **共享头文件**：`demo_ioctl.h` 驱动与用户态共用

---

## 二、W1 学习路径中的位置

### 2.1 W1 整体架构

```
W1 (字符设备基础 - day01-07)
├── day01: miscdevice 字符设备入口
├── day02: ioctl 命令定义       ← 今天
├── day03: sysfs 属性接口
├── day04: debugfs 状态快照
├── day05: waitqueue + workqueue
├── day06: 回归脚本 + 压力测试
└── day07: W1 收口 + 环境整理
```

### 2.2 Day01 vs Day02 的关系

```
Day01：打通 open → write → release 链路
       demo_write 只打日志，不处理数据

Day02：在跑通的链路上加 ioctl 实现 SET/GET
       - 用户写入一个 int，驱动保存
       - 用户读一个 int，驱动返回保存的值
       - 这就是完整的"用户态 ↔ 内核态"数据交互闭环
```

---

## 三、为什么需要 ioctl？

### 3.1 read/write 的局限性

```
read/write 适合：
  - 顺序数据流（文件、管道、socket）
  - 字节序列的读写

read/write 不适合：
  - 设置设备模式
  - 配置参数
  - 查询设备状态
  - 读写寄存器值
  - 传递结构化命令

ioctl 的本质：
  cmd（命令号） + arg（参数）
  驱动根据 cmd 决定做什么，arg 携带数据
```

### 3.2 ioctl 和 read/write 的分工

```
read/write：数据通道，适合流式读写
ioctl：     控制通道，适合配置/查询/命令

真实驱动里通常组合使用：
  ioctl(fd, SET_MODE, &mode)     → 设置模式
  ioctl(fd, GET_STATUS, &status) → 查询状态
  read(fd, buf, count)           → 读取数据
  write(fd, buf, count)          → 写入数据
```

---

## 四、ioctl 命令号设计

### 4.1 为什么命令号要规范？

```
用户态和内核态在不同的地址空间：
  - 用户态：0x0001000 ~ 0x7fffffff（用户地址）
  - 内核态：0x80000000 ~（内核地址）

如果 ioctl 命令号设计混乱：
  - 用户发 SET，驱动理解成 GET
  - 数据搬运错位，内存破坏

所以 Linux 规范了 ioctl 命令号的编码：
  ┌─────────┬────────┬────────┬────────┐
  │  31-30  │ 29-16  │ 15-8   │  7-0   │
  ├─────────┼────────┼────────┼────────┤
  │  DIR    │ SIZE   │ TYPE   │ NR     │
  └─────────┴────────┴────────┴────────┘

  TYPE：幻数（magic），标识驱动类型
  NR：  命令编号
  SIZE：参数大小
  DIR： 传输方向（R/W/RW）
```

### 4.2 Day02 的命令号定义

```c
// demo_ioctl.h（驱动和用户态共用）
#define DEMO_IOC_MAGIC  'k'        // 幻数：单个字符

// SET 命令：用户 → 内核，传输 int
#define DEMO_IOCTL_SET  _IOW(DEMO_IOC_MAGIC, 1, int)
//  _IOW(g,m,type) 展开后包含：幻数='k'，编号=1，大小=sizeof(int)

// GET 命令：内核 → 用户，传输 int
#define DEMO_IOCTL_GET  _IOR(DEMO_IOC_MAGIC, 2, int)
//  _IOR(g,m,type) 展开后包含：幻数='k'，编号=2，大小=sizeof(int)
```

```
_IOW/_IOR/_IOWR 的区别：
  _IOW：用户态写入内核（write to kernel）
  _IOR：内核态读取出来（read from kernel）
  _IOWR：双向传输（read/write）

宏展开后的值示例（32位）：
  _IOW('k', 1, int)  → 0x40046b01  （DIR=01, TYPE='k'=0x6b, NR=1, SIZE=4）
  _IOR('k', 2, int)  → 0x80046b02  （DIR=02, TYPE='k'=0x6b, NR=2, SIZE=4）
```

### 4.3 为什么必须用共享头文件？

```
驱动（demo.c）：
  #include "demo_ioctl.h"
  switch (cmd) {
      case DEMO_IOCTL_SET:  // = 0x40046b01
          ...
      case DEMO_IOCTL_GET:  // = 0x80046b02
          ...
  }

用户态（test_ioctl.c）：
  #include "demo_ioctl.h"
  ioctl(fd, DEMO_IOCTL_SET, &val);  // = 0x40046b01
  ioctl(fd, DEMO_IOCTL_GET, &val);  // = 0x80046b02

如果两边各自定义：
  驱动写 0x40046b01
  用户写 0x00000001
  → 命令号不匹配，驱动走 default 分支返回 -EINVAL
```

---

## 五、数据搬运详解

### 5.1 为什么不能直接访问用户态指针？

```
用户态地址空间的特点：
  - 用户态指针是虚拟地址，在内核态直接解引用可能：
    a) 触发页面错误（地址不在物理内存）
    b) 读到错误的值（地址属于另一个进程）
    c) 触发权限错误（内核态权限更高但格式不对）

内核不能假设用户态指针是有效的！

所以 Linux 提供受控拷贝接口：
  copy_from_user(dst, src, n)  → 内核 ← 用户
  copy_to_user(dst, src, n)    → 内核 → 用户
```

### 5.2 copy_from_user 的使用

```c
static long demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int __user *user_ptr = (int __user *)arg;  // 用户态指针
    int temp;

    switch (cmd) {
    case DEMO_IOCTL_SET:
        // 从用户态拷贝 4 字节到内核 temp
        if (copy_from_user(&temp, user_ptr, sizeof(temp)))
            return -EFAULT;  // 拷贝失败（无效地址）
        kernel_value = temp;
        printk("Set value to %d\n", kernel_value);
        break;
    ...
    }
}
```

```
copy_from_user 的返回值：
  - 返回 0：拷贝成功
  - 返回非 0：还有多少字节没拷贝成功

if (copy_from_user(&temp, user_ptr, sizeof(temp)))
    return -EFAULT;

这个 if 判断等价于"如果返回值非 0，说明地址无效"

EFAULT 的语义：
  - "Bad address"（坏地址）
  - 告诉用户态"你给我的地址我访问不了"
```

### 5.3 copy_to_user 的使用

```c
case DEMO_IOCTL_GET:
    // 从内核 kernel_value 拷贝 4 字节到用户态
    if (copy_to_user(user_ptr, &kernel_value, sizeof(kernel_value)))
        return -EFAULT;
    printk("Get value %d\n", kernel_value);
    break;
```

---

## 六、Day02 驱动代码分析

### 6.1 demo_ioctl 的完整实现

```c
static long demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int __user *user_ptr = (int __user *)arg;
    int temp;

    switch (cmd) {
    case DEMO_IOCTL_SET:
        // arg 是用户态传来的 int 地址
        if (copy_from_user(&temp, user_ptr, sizeof(int)))
            return -EFAULT;
        kernel_value = temp;
        printk(KERN_INFO "Demo: Set value to %d\n", kernel_value);
        break;

    case DEMO_IOCTL_GET:
        // arg 是用户态要写入的 int 地址
        if (copy_to_user(user_ptr, &kernel_value, sizeof(int)))
            return -EFAULT;
        printk(KERN_INFO "Demo: Get value %d\n", kernel_value);
        break;

    default:
        printk(KERN_WARNING "Demo: Unknown ioctl command 0x%x\n", cmd);
        return -EINVAL;
    }
    return 0;
}
```

### 6.2 为什么用 unlocked_ioctl？

```c
static struct file_operations demo_fops = {
    .owner          = THIS_MODULE,
    .open           = demo_open,
    .release        = demo_release,
    .unlocked_ioctl = demo_ioctl,   ← 这是 2.6.36+ 的接口
};
```

```
Linux 2.6.36 之前：
  int (*ioctl)(struct inode *, struct file *, unsigned int, unsigned long);

Linux 2.6.36+：
  long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);

变化原因：
  - 旧接口需要 BKL（Big Kernel Lock），性能差
  - 新接口不需要 BKL，更适合多核并发

我们现在的内核是 5.15，用 unlocked_ioctl。
```

### 6.3 kernel_value 的角色

```c
static int kernel_value = 0;  // 内核维护的设备状态
```

```
kernel_value 代表的是"设备内部状态"的简化模拟。

真实硬件驱动里，这可能是：
  - 设备的配置寄存器值
  - 工作模式标志
  - 统计计数器
  - 缓冲区指针

Day02 的 SET/GET 就是对这一个 int 的写入和读出。
这是最简化的"驱动 ↔ 用户态"数据交互模型。
```

---

## 七、Day02 vs Day03 的关系

### 7.1 Day02 的局限

```
Day02 的 SET/GET 只操作一个 int 变量：
  - kernel_value 只是一个 int
  - 没有保存到设备结构体
  - 没有并发保护
  - 没有 sysfs 导出

Day03 要解决：
  - 把状态收敛到 struct demo_device
  - 增加 enable 开关
  - 增加 counter 统计
  - 通过 sysfs 导出 enable/counter
```

### 7.2 从 Day02 到 Day03 的演进

```
Day02：
  static int kernel_value = 0;  // 全局变量
  → 没封装到设备对象
  → 没有控制面

Day03：
  struct demo_device {
      int enable;
      int counter;
  };
  static struct demo_device *g_demo;
  → 封装到设备对象
  → sysfs 提供 enable 控制面
  → sysfs 提供 counter 状态面
  → ioctl 只做功能面
```

---

## 八、面试要会讲的五句话

1. **"ioctl 的本质是'命令 + 参数'模式，适合做配置、查询、模式切换等控制类操作，而 read/write 适合流式数据读写；Linux 通过 _IO/_IOR/_IOW/_IOWR 宏规范命令号编码，避免用户态和内核态对命令号理解不一致"**
   → 理解 ioctl vs read/write 的分工

2. **"copy_from_user/copy_to_user 是内核提供的安全拷贝接口，因为用户态地址在内核态不能直接解引用；copy_from_user 返回非零表示地址无效，返回 -EFAULT；这是 Linux 用户态/内核态隔离的基本规则"**
   → 理解数据搬运的安全机制

3. **"ioctl 命令号必须驱动和用户态共用同一套定义（demo_ioctl.h），否则用户发 0x40046b01、驱动认为是 0x00000001，命令号不匹配会走 default 分支返回 -EINVAL"**
   → 理解共享头文件的必要性

4. **"unlocked_ioctl 是 2.6.36+ 的接口，取消了旧版 ioctl 需要的 BKL（Big Kernel Lock），更适合多核并发；我们用的 5.15 内核用的是 unlocked_ioctl"**
   → 理解 unlocked_ioctl 的背景

5. **"Day02 和 Day03 的关系是：Day02 建立'用户态 ↔ 内核态'的数据交互协议（SET/GET），Day03 在此基础上增加 sysfs 控制面（enable/counter）和设备对象封装（struct demo_device）；骨架是 ioctl，功能在叠加"**
   → 理解 Day02 → Day03 的演进

---

## 九、验收标准

### 9.1 编译验收

- [ ] `demo.ko` 编译成功
- [ ] `test_ioctl`（静态链接）编译成功

### 9.2 SET 验收

- [ ] `/bin/test_ioctl` 运行成功
- [ ] dmesg 显示 `Set value to 88`

### 9.3 GET 验收

- [ ] SET 之后 GET 输出正确值
- [ ] dmesg 显示 `Get value 88`

### 9.4 错误路径验收

- [ ] 发送未定义命令返回 `-EINVAL`
- [ ] `dmesg | grep "Unknown ioctl command"`

---

## 附录：ioctl 命令号编码详解

```
_IOW(DEMO_IOC_MAGIC, 1, int)  展开过程：

1. DEMO_IOC_MAGIC = 'k' = 0x6b

2. _IOW 定义（linux/kbuild.h）：
   #define _IOW(type,nr,t)  _IOC(_IOC_WRITE, type, nr, sizeof(t))

3. _IOC 定义：
   #define _IOC(dir, type, nr, size) \
       (((dir)  << _IOC_DIRBITS) | \
        ((type) << _IOC_TYPBITS) | \
        ((nr)   << _IOC_NRBITS)  | \
        ((size) << _IOC_SIZEBITS))

4. _IOC_WRITE = 1，_IOC_READ = 2

5. 代入：
   dir=1, type='k'=0x6b, nr=1, size=4
   = (1 << 30) | (0x6b << 8) | (1 << 0) | (4 << 16)
   = 0x40000000 | 0x00006b00 | 0x00000001 | 0x00040000
   = 0x40046b01

这就是为什么 DEMO_IOCTL_SET = 0x40046b01：
  0x40 = 0100 0000 = DIR=01(写), 其他位=0
  0x6b = 'k'     = TYPE
  0x01 = NR      = 命令号1
  0x02 = SIZE    = 4字节（sizeof(int)）
```
