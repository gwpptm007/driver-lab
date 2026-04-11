# Day05 深度指南 - waitqueue 阻塞读 + workqueue 异步处理 + 单槽模型

## 一、Day05 是什么？

Day05 是 W1（字符设备基础）的第五天，定位是**waitqueue + workqueue + 并发保护三位一体**。

**核心目标**：建立"阻塞等待 → 异步处理 → 唤醒读者"的完整闭环，同时掌握 waitqueue/workqueue/mutex 三种内核基本同步原语。

Day05 不做 platform 总线，不做 Device Tree。它的重点是：
1. **waitqueue**：`wait_event_interruptible()` 阻塞 + `wake_up_interruptible()` 唤醒
2. **workqueue**：`INIT_WORK()` + `schedule_work()` 异步提交 + `cancel_work_sync()` 卸载同步
3. **mutex**：保护所有共享状态路径
4. **单槽 pending work 模型**：同一时刻只允许一个 work，返回 -EBUSY
5. **上下文切换**：进程上下文 vs workqueue 上下文的区别

---

## 二、W1 学习路径中的位置

### 2.1 W1 整体架构

```
W1 (字符设备基础 - day01-07)
├── day01: miscdevice 字符设备入口
├── day02: ioctl 命令定义
├── day03: sysfs 属性接口
├── day04: debugfs 调试快照
├── day05: waitqueue + workqueue      ← 今天
├── day06: 回归脚本 + 压力测试
└── day07: W1 收口 + 环境整理
```

### 2.2 Day05 与前后天的关系

```
Day04 vs Day05：
  - Day04：sysfs enable/counter、debugfs status/log_level
  - Day05：在同结构基础上增加 waitqueue + workqueue
  - Day05 复用 Day04 的 struct demo_device、sysfs、debugfs

Day05 vs Day06：
  - Day05：单次 write → work → read 能跑通
  - Day06：在 Day05 基础上做 500 次回归 + 并发压测
  - Day06 验证 Day05 的代码是否"稳"

Day05 是 W1 的"并发基础课"，Day06 是"稳定性验证课"
```

---

## 三、为什么需要 waitqueue？

### 3.1 忙等 vs 阻塞

```
忙等（polling）的写法：
  while (!data_ready) {
      // 一直占用 CPU
      ; // no-op
  }
  read(data);

问题：
  - 浪费 CPU 资源
  - 如果条件一直不满足，进程永远不释放 CPU
  - 用户态表现为"卡死"

阻塞（waitqueue）的写法：
  ret = wait_event_interruptible(dev->read_wq, dev->data_ready);
  if (ret)
      return ret;  // 被信号唤醒
  read(data);

优点：
  - 进程睡眠，不占用 CPU
  - 条件满足时被唤醒
  - 收到信号可以退出
```

### 3.2 Day05 的阻塞读设计

```
用户执行：read(fd, buf, 128)

驱动层面：
  1. read() 检查 data_ready
  2. 如果 data_ready=false，调用 wait_event_interruptible() 睡眠
  3. 进程进入 TASK_INTERRUPTIBLE 状态
  4. CPU 调度去跑其他任务

workqueue 处理完成后：
  1. work_handler 设置 data_ready=true
  2. 调用 wake_up_interruptible(&dev->read_wq)
  3. 唤醒等待队列上的进程
  4. read() 醒来，二次检查 data_ready
  5. 确认 true 后，copy_to_user() 返回数据
```

---

## 四、workqueue 详解

### 4.1 为什么需要 workqueue？

```
用户 write() 的诉求：把数据交给驱动后尽快返回

如果 write() 直接做慢处理（msleep 500ms）：
  - 用户态要等 500ms 才能继续
  - 用户体验差

workqueue 的思路：
  write() 只负责"登记请求 + 提交 work"
  立即返回给用户
  慢处理交给 worker 线程异步执行

这样 write() 的延迟 ≈ 提交 work 的时间（微秒级）
而不是真正处理的时间（毫秒级）
```

### 4.2 Day05 的 workqueue 流程

```
write() 路径：
  1. 检查 enable/work_pending
  2. copy_from_user() 接收数据
  3. 填充 input_buf，设置 work_pending=true
  4. schedule_work(&dev->work)  ← 立即返回
  5. 返回 count 给用户态

work_handler 路径（worker 线程）：
  1. msleep(500) 模拟慢处理
  2. 生成 output_buf（"processed: xxx"）
  3. 设置 data_ready=true, work_pending=false
  4. wake_up_interruptible(&dev->read_wq)  ← 唤醒读者
```

### 4.3 workqueue 的关键 API

```c
// 初始化 work
INIT_WORK(&dev->work, demo_work_handler);

// 提交 work（立即返回，不等待完成）
schedule_work(&dev->work);

// 取消 work（如果在队列里）或等待其完成（如果在执行）
cancel_work_sync(&dev->work);
```

```
schedule_work() 的特点：
  - 立即返回（只负责排队）
  - 不保证 work 已完成
  - worker 线程异步执行

cancel_work_sync() 的语义：
  - 如果 work 还没执行：取消并返回
  - 如果 work 正在执行：等待执行完再返回
  - 确保 module exit 时 worker 已完全退出

cancel_work_sync() 是防止 UAF 的关键！
```

---

## 五、单槽 pending work 模型

### 5.1 为什么要限制单槽？

```
Day05 的简化设计：同一时刻只允许一个 pending work

原因：
  1. input_buf/output_buf 只有一份
  2. 如果多个 work 同时跑，会相互覆盖
  3. 简化了同步问题（不需要队列管理）

代价：
  - 第二个 write() 在 work_pending=true 时返回 -EBUSY
  - 这不是 bug，是设计决定的

在并发压测中：
  - err = 0（真正失败）
  - busy > 0（单槽竞争，预期行为）
```

### 5.2 单槽模型的状态机

```
work_pending 状态机：

  初始：work_pending = false
  │
  ├─ write/ioctl(SET)
  │    work_pending = true
  │    schedule_work()
  │    return count/0
  │
  ├─ [work_handler 执行中]
  │    work_pending = true
  │
  ├─ work_handler 完成
  │    work_pending = false
  │    data_ready = true
  │
  └─ read() 取走数据
       data_ready = false

在 work_pending=true 期间：
  新 write() → return -EBUSY
  新 ioctl(SET) → return -EBUSY
```

---

## 六、Day05 核心代码分析

### 6.1 struct demo_device 的并发相关字段

```c
struct demo_device {
    /* 字符设备基础成员 */
    dev_t devt;
    struct cdev cdev;
    struct class *class;
    struct device *device;

    /* 并发保护锁 —— 保护所有共享状态 */
    struct mutex lock;

    /* 等待队列头 —— 阻塞读 */
    wait_queue_head_t read_wq;

    /* 工作队列项 —— 异步处理 */
    struct work_struct work;

    /* 设备开关 —— 通过 sysfs enable 控制 */
    bool enable;

    /* data_ready 表示"输出结果已准备好" */
    bool data_ready;

    /* work_pending 表示"是否有 work 在处理中" */
    bool work_pending;

    /* ioctl GET/SET 对应的整型状态值 */
    int value;

    /* 输入/输出缓冲区 */
    char input_buf[DEMO_BUF_SIZE];
    char output_buf[DEMO_BUF_SIZE];
    size_t output_len;

    /* 累计处理次数 */
    unsigned int counter;

    /* 日志级别（debugfs 导出）*/
    unsigned int log_level;

    /* debugfs 目录 */
    struct dentry *debugfs_dir;
};
```

### 6.2 demo_read() 的阻塞等待

```c
static ssize_t demo_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    struct demo_device *dev = file->private_data;
    ssize_t ret;
    size_t len;

    if (*ppos != 0)
        return 0;  // 只支持一次性读取

    // 阻塞等待，直到 data_ready == true
    ret = wait_event_interruptible(dev->read_wq, dev->data_ready);
    if (ret)
        return ret;  // 被信号打断

    mutex_lock(&dev->lock);

    // 醒来后二次检查（spurious wakeup 防护）
    if (!dev->data_ready) {
        mutex_unlock(&dev->lock);
        return -EAGAIN;
    }

    // copy_to_user 返回数据
    len = min(count, dev->output_len);
    if (copy_to_user(buf, dev->output_buf, len)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    dev->data_ready = false;  // 数据已取走，重置
    *ppos += len;

    mutex_unlock(&dev->lock);
    return len;
}
```

### 6.3 demo_write() 的异步提交

```c
static ssize_t demo_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
    struct demo_device *dev = file->private_data;
    size_t len;

    mutex_lock(&dev->lock);

    if (!dev->enable) {
        mutex_unlock(&dev->lock);
        return -EPERM;
    }

    // 单槽模型：如果已有 pending work，拒绝
    if (dev->work_pending) {
        mutex_unlock(&dev->lock);
        return -EBUSY;
    }

    // 接收数据
    len = min(count, (size_t)(DEMO_BUF_SIZE - 1));
    memset(dev->input_buf, 0, sizeof(dev->input_buf));
    memset(dev->output_buf, 0, sizeof(dev->output_buf));
    dev->output_len = 0;
    dev->data_ready = false;

    if (copy_from_user(dev->input_buf, buf, len)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }
    dev->input_buf[len] = '\0';
    dev->work_pending = true;

    mutex_unlock(&dev->lock);

    // 提交异步 work，立即返回
    schedule_work(&dev->work);

    return count;
}
```

### 6.4 demo_work_handler() 的异步处理

```c
static void demo_work_handler(struct work_struct *work)
{
    struct demo_device *dev = container_of(work, struct demo_device, work);

    if (READ_ONCE(dev->log_level))
        pr_info("demo: workqueue start\n");

    // 模拟慢处理（500ms）
    msleep(500);

    mutex_lock(&dev->lock);

    // 如果处理期间设备被关闭
    if (!dev->enable) {
        dev->work_pending = false;
        mutex_unlock(&dev->lock);
        wake_up_interruptible(&dev->read_wq);  // 唤醒，避免读睡死
        return;
    }

    // 生成输出
    scnprintf(dev->output_buf, DEMO_BUF_SIZE, "processed: %s", dev->input_buf);
    dev->output_len = strnlen(dev->output_buf, DEMO_BUF_SIZE);
    dev->data_ready = true;
    dev->work_pending = false;
    dev->counter++;

    mutex_unlock(&dev->lock);

    // 唤醒阻塞的读者
    wake_up_interruptible(&dev->read_wq);

    if (READ_ONCE(dev->log_level))
        pr_info("demo: workqueue done, reader woken up\n");
}
```

---

## 七、上下文切换详解

### 7.1 进程上下文 vs 中断上下文

```
open/read/write/ioctl：
  - 由用户态系统调用触发
  - 运行在当前进程的上下文中
  - 可以睡眠
  - 可以用 mutex/semaphore
  - 可以直接访问用户态内存（通过 copy_from_user）

workqueue handler：
  - 由内核 worker 线程执行
  - 也是可睡眠上下文
  - 可以 msleep
  - 可以 mutex_lock

区别：
  进程上下文：与特定进程关联
  workqueue：跑在内核线程里，不与用户进程绑定
```

### 7.2 为什么 workqueue 可以睡眠？

```
因为 workqueue 运行在进程上下文（worker 线程），
而不是中断上下文！

中断上下文（hardirq）的限制：
  - 不能睡眠
  - 不能调用可能睡眠的函数
  - 只能用 spinlock（不可睡眠）

workqueue handler（softirq / 进程上下文）：
  - 可以睡眠
  - 可以 msleep
  - 可以 mutex_lock

Day05 的 demo_work_handler 模拟的是：
  把慢处理下沉到 worker 线程，
  避免阻塞用户态 write() 调用路径
```

### 7.3 mutex vs spinlock

```
Day05 为什么用 mutex 而不是 spinlock？

spinlock（不可睡眠，适合中断上下文）：
  - 如果锁被占用，CPU 忙等
  - 不能在持有 spinlock 时睡眠
  - 适合保护非常短小的临界区

mutex（可睡眠，适合进程/workqueue 上下文）：
  - 如果锁被占用，进程睡眠
  - 可以在持有 mutex 时睡眠
  - 适合保护较长的临界区

Day05 的临界区：
  read/write/work_handler 都要访问 dev->data_ready/dev->work_pending
  这些路径都可能睡眠，所以用 mutex
```

---

## 八、Day05 vs Day06 的关系

### 8.1 Day06 验证 Day05 的稳定性

```
Day05 建立的能力：
  - waitqueue 阻塞等待
  - workqueue 异步处理
  - mutex 并发保护
  - cancel_work_sync() 卸载同步

Day06 的验证：
  - 500 次 insmod/rmmod 验证 init/remove 干净
  - 并发压测 5 分钟验证 mutex 保护有效
  - cancel_work_sync() 确保卸载时 worker 不访问已释放内存

"功能能跑" ≠ "代码稳"
Day06 解决的就是这个问题
```

### 8.2 cancel_work_sync() 的必要性

```c
static void __exit demo_exit(void)
{
    if (!g_demo)
        return;

    // 关键：卸载前同步取消 work
    cancel_work_sync(&g_demo->work);

    debugfs_remove_recursive(g_demo->debugfs_dir);
    device_remove_file(g_demo->device, &dev_attr_counter);
    device_remove_file(g_demo->device, &dev_attr_enable);
    device_destroy(g_demo->class, g_demo->devt);
    class_destroy(g_demo->class);
    cdev_del(&g_demo->cdev);
    unregister_chrdev_region(&g_demo->devt, 1);

    kfree(g_demo);
}
```

```
如果不调用 cancel_work_sync()：
  1. rmmod 开始卸载模块
  2. kfree(g_demo) 释放内存
  3. 但 worker 线程可能还在访问 g_demo
  4. → UAF（Use-After-Free）触发

cancel_work_sync() 确保：
  - 如果 work 还没执行：取消
  - 如果 work 正在执行：等待执行完
  - 两者都返回后，g_demo 才能安全释放
```

---

## 九、面试要会讲的五句话

1. **"waitqueue 用于进程阻塞等待：当条件不满足时，wait_event_interruptible() 让进程进入 TASK_INTERRUPTIBLE 睡眠，释放 CPU；当条件满足时，wake_up_interruptible() 唤醒进程；阻塞等待比忙等（while loop）节省 CPU资源"**
   → 理解 waitqueue 的阻塞机制

2. **"workqueue 用于把慢处理下沉到 worker 线程：write() 只负责登记请求 + schedule_work()，立即返回给用户；真正的 msleep(500) 在 worker 线程里异步执行；schedule_work() 立即返回，不等待 work 完成"**
   → 理解 workqueue 的异步处理模型

3. **"cancel_work_sync() 在模块卸载前调用，确保 worker 线程已完全退出，防止 UAF（Use-After-Free）；它的语义是'如果 work 在跑就等它跑完'，是驱动卸载安全的关键API"**
   → 理解 cancel_work_sync() 的防 UAF 作用

4. **"单槽 pending work 模型下，同一时刻只允许一个 work 在跑，新的 write/ioctl(SET) 返回 -EBUSY；这是设计决定不是 bug，压测时 err=0 才是真正失败，busy>0 是预期的锁竞争"**
   → 理解单槽模型和压测通过标准

5. **"Day05 和 Day06 的关系是'功能'和'稳定性'的分工：Day05 建立 waitqueue + workqueue + mutex 的并发基础，Day06 用 500 次回归和并发压测验证 Day05 代码的稳定性；'功能能跑'不等于'代码稳'，这是 Day06 要解决的核心问题"**
   → 理解 Day05 → Day06 的关系

---

## 十、验收标准

### 10.1 阻塞读验收

- [ ] `echo hello > /dev/demo` 后立即返回（不卡住）
- [ ] `cat /dev/demo` 约 500ms 后返回 "processed: hello"

### 10.2 后台读 + 触发唤醒验收

- [ ] `/bin/test read &` 后台读先阻塞
- [ ] `/bin/test write hello` 触发唤醒
- [ ] 后台 cat 正确输出处理结果

### 10.3 单槽模型验收

- [ ] 连续多次 write 第二次返回 -EBUSY（如果第一次还没完成）
- [ ] ioctl SET 同样返回 -EBUSY

### 10.4 稳定性验收

- [ ] `rmmod demo.ko` 成功
- [ ] dmesg 无 UAF/leak/Oops 告警
- [ ] 多次 write/read 循环不崩溃

---

## 附录：Day05 完整状态流转图

```
用户态 write()                        workqueue handler              用户态 read()
==============                        ================                ==============

write("/dev/demo", "hello")
  │                                        │                              │
  ├─ mutex_lock(&dev->lock)                │                              │
  ├─ 检查 enable=1, work_pending=0         │                              │
  ├─ copy_from_user(input_buf, "hello")    │                              │
  ├─ work_pending = true                   │                              │
  ├─ schedule_work(&dev->work) ──────────► │                              │
  ├─ return count（立即返回！）             │                              │
  │                                        │                              │
  │                                 worker 线程异步执行                     │
  │                                        ├─ msleep(500)                  │
  │                                        ├─ mutex_lock(&dev->lock)       │
  │                                        ├─ 生成 "processed: hello"      │
  │                                        ├─ data_ready = true            │
  │                                        ├─ work_pending = false         │
  │                                        ├─ wake_up_interruptible() ────►│
  │                                        │                              │
  │                                 return（继续其他任务）                    │
  │                                        │                              │
  │                                        │                          read(/dev/demo)
  │                                        │                              │
  │                                        │                              ├─ wait_event_interruptible(data_ready=true)  ← 被唤醒！
  │                                        │                              ├─ mutex_lock(&dev->lock)
  │                                        │                              ├─ 二次检查 data_ready
  │                                        │                              ├─ copy_to_user(output_buf)
  │                                        │                              ├─ data_ready = false
  │                                        │                              ├─ return len
```
