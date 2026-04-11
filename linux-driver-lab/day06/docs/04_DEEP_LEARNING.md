# Day06 深度指南 - 回归脚本 / 卸载验证 / 并发压力测试

## 一、Day06 是什么？

Day06 是 W1（字符设备基础）的第六天，定位是**驱动质量自动化验证**。

**核心目标**：用自动化脚本验证驱动的稳定性——反复装卸不留下脏状态、并发读写不出现竞态崩溃、dmesg 无异常告警。

Day06 不新增驱动功能。它的重点是：
1. **500 次 insmod/rmmod 回归**：验证模块初始化/卸载路径是否干净
2. **5 分钟并发压测**：2 reader + 2 writer 同时跑，验证竞态安全
3. **dmesg 异常关键字扫描**：Oops/BUG/KASAN/UAF/leak 等告警自动捕获
4. **cancel_work_sync()**：确保卸载时 worker 不再访问已释放内存

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
├── day06: 回归脚本 + 压力测试        ← 今天
└── day07: W1 收口 + 环境整理
```

### 2.2 Day06 与前后天的关系

```
Day05 vs Day06：
  - Day05：把 waitqueue + workqueue + sysfs + debugfs 全部实现
  - Day06：把 Day05 的能力用于自动化回归验证

Day06 vs Day07：
  - Day06：验证代码是否"稳"
  - Day07：整理文档和环境，让项目可迁移

Day06 是 W1 的"质量关卡"，
Day07 是 W1 的"文档收口"
```

---

## 三、为什么 Day06 突然转向"回归测试"？

### 3.1 功能能跑 ≠ 代码稳

```
很多驱动在"手点两下功能"时看起来是好的，
真正的问题往往出现在：

  重复加载/卸载后：资源没有清干净
  并发读写时：状态机混乱，出现偶发性错误
  卸载时：worker 还在跑，触发 UAF
  用户态只覆盖 happy path，没有覆盖 -EBUSY / 超时场景
```

### 3.2 Day06 的核心价值

```
从"功能能跑"进入"代码是否稳"

Day06 把风险点系统化地测一遍：
  1. 反复装卸 500 次 —— 验证 init/remove 路径
  2. 并发压测 5 分钟 —— 验证竞态安全
  3. dmesg 扫描 —— 捕获 Oops/UAF/leak
  4. cancel_work_sync —— 确保卸载时 worker 不访问已释放内存
```

---

## 四、Day05 的代码基线

### 4.1 仍然沿用的核心机制

```
waitqueue（阻塞读）：
  - read() 没有数据时，wait_event_interruptible() 睡眠
  - workqueue 完成后，wake_up_interruptible() 唤醒

workqueue（异步处理）：
  - write/ioctl(SET) 只登记请求，schedule_work() 提交
  - worker 线程异步执行 msleep(500) 模拟慢处理
  - 完成后唤醒阻塞的 reader

mutex（并发保护）：
  - 所有访问共享状态的路径都加锁
  - 包括 read/write/ioctl/work_handler
```

### 4.2 单槽 pending work 模型

```
当前驱动的设计：同一时刻只允许一个 pending work

如果上一个 work 还没做完，新的 write/ioctl(SET) 返回 -EBUSY

这在并发压测时会表现为：
  - busy 计数非 0（预期行为，不是 bug）
  - err 计数必须为 0（才是真正的失败）
```

---

## 五、cancel_work_sync() 为什么关键？

### 5.1 不用它会怎样？

```
模块卸载时：
  - 模块内存即将被释放
  - 但 worker 线程可能还在访问 g_demo 指针
  - 触发 UAF（Use-After-Free）

这是驱动卸载时最常见的内存安全问题之一
```

### 5.2 cancel_work_sync() 的语义

```c
// demo_exit() 中
cancel_work_sync(&g_demo->work);
```

```
它不是"仅仅取消排队"，而是：
  - 如果 work 还没执行：取消它
  - 如果 work 已经在执行：等待它执行完再返回

这样确保：
  - worker 退出时，g_demo 还没被 kfree
  - kfree 执行时，worker 已经不可能再访问 g_demo
```

### 5.3 卸载日志的正确顺序

```
[  104.799875] demo_pdrv: module exit
[  104.800295] demo_pdrv demo_pdrv: remove
[  104.800940] demo_pdrv demo_pdrv: devm cleanup
[  104.801895] demo_pdrv: platform_device release
```

```
cancel_work_sync() 在 device_destroy() / kfree() 之前执行
→ 确保 worker 先完全退出
→ 再释放内存
```

---

## 六、四脚本分工

### 6.1 insmod_rmmod.sh（500 次装卸回归）

```
核心动作：
  for i in $(seq 1 N); do
    insmod demo.ko
    set/get/read_timeout  // 一轮功能验证
    rmmod demo
  done

验证什么：
  - insmod 能否成功创建 /dev/demo
  - 功能接口是否正常
  - rmmod 是否能干净卸载
  - 无 Oops/UAF/leak 告警
```

### 6.2 stress_rw.sh（并发压测）

```
默认：300 秒（5 分钟）
并发：2 writer + 2 reader

为什么用 read_timeout：
  - reader 用 wait_event_interruptible() 阻塞
  - 如果用普通 read，压测脚本可能永久挂住
  - read_timeout 用 alarm + sigaction 实现超时退出

允许出现的现象（不是 bug）：
  - busy 计数非 0（单槽模型下的预期竞争）
  - timeout 计数非 0（reader 定时阻塞等待）

不允许出现的现象（真正的失败）：
  - err 计数非 0
  - Oops/BUG/UAF/leak
```

### 6.3 check_dmesg.sh（dmesg 异常扫描）

```
扫描关键字：
  Oops / BUG / KASAN / UAF / leak / WARNING

为什么重要：
  - 很多内核问题不导致用户态报错
  - 但 dmesg 里已经有异常痕迹
  - 自动化扫描比手工 dmesg | grep 更可靠
```

### 6.4 all.sh（一键串起所有验收）

```
执行顺序：
  1. insmod_rmmod.sh 500
  2. insmod demo.ko（重新加载，准备压测）
  3. stress_rw.sh 300
  4. check_dmesg.sh

一键 all.sh 走完 = Day06 验收通过
```

---

## 七、并发压测中的 -EBUSY 为什么不是 bug？

### 7.1 单槽模型的语义

```
write/ioctl(SET) 路径：
  if (work_pending)
      return -EBUSY

这意味着：
  - 上一批请求还没处理完
  - 新的请求被拒绝，而不是排队

在压测中：
  - 多线程同时写
  - 大部分线程会撞上 busy
  - 这是设计决定的，不是异常
```

### 7.2 压测通过标准

```
busy 计数：允许非 0
timeout 计数：允许非 0
err 计数：必须为 0

真正应该关注的是：
  - 脚本是否正常退出（没有被信号打断）
  - dmesg 是否有异常
  - /dev/demo 是否仍然可访问
```

---

## 八、Day06 与 W2 的关系

### 8.1 W1 收口，W2 开启

```
W1（day01-07）：字符设备基础
  - 重点：/dev 接口、waitqueue、workqueue、sysfs/debugfs
  - Day06：质量验证

W2（day08-14）：嵌入式驱动模式
  - 重点：platform_driver、Device Tree、IRQ、regmap、ftrace
  - Day08：从字符设备转向 platform 总线模型

Day06 是 W1 最后一个"技术验证日"，
Day07 做文档收口，
Day08 切换到 W2
```

### 8.2 cancel_work_sync 在 W2 仍然关键

```
Day08 之后的 platform_driver 场景中：
  - 驱动卸载时可能还有 workqueue 在跑
  - 如果不 cancel_work_sync，可能触发 UAF
  - Day06 已经建立的"卸载前同步等待"习惯，在 W2 仍然适用
```

---

## 九、面试要会讲的五句话

1. **"Day06 的核心是用自动化脚本验证驱动的稳定性：500 次 insmod/rmmod 验证 init/remove 路径干净，5 分钟并发压测验证竞态安全，dmesg 扫描捕获 Oops/UAF/leak 等内核告警"**
   → 理解 Day06 的定位

2. **"cancel_work_sync() 在模块卸载前执行，确保 worker 线程先完全退出，再释放 g_demo 内存，防止 UAF（Use-After-Free）；它的语义是'如果 work 还在跑就等它跑完'"**
   → 理解 cancel_work_sync 的价值

3. **"单槽 pending work 模型下，busy 计数非 0 是预期行为，不是 bug；因为同一时刻只允许一个 work 在跑，新的请求会返回 -EBUSY；压测通过标准是 err=0 而不是 busy=0"**
   → 理解单槽模型和压测通过标准

4. **"check_dmesg.sh 扫描 Oops/BUG/KASAN/UAF/leak 等关键字，因为很多内核问题不导致用户态报错，但 dmesg 里已经有异常痕迹，自动化扫描比手工 grep 更可靠"**
   → 理解 dmesg 扫描的必要性

5. **"Day06 把 Day05 的 waitqueue + workqueue + sysfs + debugfs 能力用于自动化回归验证，验证'功能能跑'不等于'代码稳'；Day07 做 W1 文档收口，Day08 进入 W2 platform_driver 模式"**
   → 理解 Day06 在 W1 和整体学习路径中的位置

---

## 十、验收标准

### 10.1 装卸回归验收

- [ ] insmod_rmmod.sh 500 次循环全部 PASS
- [ ] 每次 insmod 后 /dev/demo 存在
- [ ] 每次 rmmod 后无残留资源

### 10.2 并发压测验收

- [ ] stress_rw.sh 300 秒正常退出
- [ ] err 计数 = 0
- [ ] busy 计数允许非 0（单槽模型预期行为）

### 10.3 dmesg 验收

- [ ] check_dmesg.sh 输出 `PASS: no suspicious dmesg pattern found`
- [ ] 无 Oops / Call Trace / UAF / leak 告警

### 10.4 驱动状态验收

- [ ] 压测后 `cat /sys/kernel/debug/demo_debug/status` 仍可读
- [ ] 驱动在压测后仍保持可观察状态

---

## 附录：Day06 脚本快速命令

```
# 一键验收
/bin/all.sh

# 分步验收
/bin/insmod_rmmod.sh 500
insmod /demo.ko
/bin/stress_rw.sh 300
/bin/check_dmesg.sh

# 手工残留检查
ls /dev/demo
cat /sys/kernel/debug/demo_debug/status
dmesg | grep -i "demo\|uaf\|leak\|oops"
```
