# Day05 - waitqueue / workqueue / 并发与上下文

## 1. 学习目标

今天的目标：

- 理解 waitqueue 的作用：阻塞等待，而不是忙等
- 理解 workqueue 的作用：异步处理，把重活从当前路径下沉出去
- 区分进程上下文与 workqueue 上下文
- 学会在共享状态上加锁，避免并发问题
- 完成一个“写入 -> 异步处理 -> 唤醒读取”的最小驱动闭环

本日对应学习计划：

- 任务：并发：waitqueue/workqueue；模拟阻塞/唤醒；强调上下文
- 产出/验收：并发压测不死锁；注释说明上下文

---

## 2. 相关理论

### 2.1 什么是 waitqueue

waitqueue（等待队列）用于让进程在“条件不满足”时睡眠，
而不是一直 while 循环占用 CPU 忙等。

本实验中：

- `read()` 在没有数据时阻塞
- `workqueue` 处理完成后唤醒 `read()`

核心接口：

- `init_waitqueue_head()`
- `wait_event_interruptible()`
- `wake_up_interruptible()`

---

### 2.2 什么是 workqueue

workqueue 用于把工作异步交给内核 worker 线程执行。

本实验中：

- `write()` / `ioctl(SET)` 不直接生成结果
- 只负责提交 work
- work handler 中模拟慢处理（`msleep(500)`）
- 完成后设置 `data_ready = true`
- 唤醒阻塞在 `read()` 的进程

核心接口：

- `INIT_WORK()`
- `schedule_work()`
- `cancel_work_sync()`

---

### 2.3 上下文说明

#### 进程上下文
`open/read/write/ioctl` 都运行在进程上下文，因为它们由用户态系统调用触发。

特点：

- 可以睡眠
- 可以用 mutex
- 可以 copy_to_user / copy_from_user

#### workqueue 上下文
work handler 运行在内核 worker 线程中，也是可睡眠上下文。

特点：

- 可以 `msleep()`
- 可以 `mutex_lock()`
- 适合放慢操作、异步处理

---

### 2.4 为什么这里用 mutex

本实验的共享状态主要在：

- `read()`
- `write()`
- `workqueue handler`

这些路径都在可睡眠上下文，所以这里使用 `mutex` 最合适。
本实验还没有进入硬中断 top-half，不需要 `spinlock`。

---

## 3. 关键文件说明

- `demo.c`：驱动主体，实现 cdev、read/write/ioctl、sysfs、debugfs、waitqueue、workqueue
- `Makefile`：编译内核模块和用户态测试程序
- `test.c`：用户态测试工具，用于 read/write/ioctl 验证
- `build.sh`：构建 rootfs，启动 QEMU
- `rootfs.img`：构建出的 initramfs 镜像（构建后生成）

---

## 4. 编译与启动

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day05
chmod +x build.sh
./build.sh
```

---

## 5. 功能测试步骤

### 5.1 查看设备和接口

进入 QEMU 后执行：

```sh
ls -l /dev/demo
ls /sys/class/demo_class/demo
ls /sys/kernel/debug/demo_debug
cat /sys/kernel/debug/demo_debug/status
```

预期：

- `/dev/demo` 存在
- sysfs 属性存在：`enable`、`counter`
- debugfs 中存在：`status`、`log_level`

---

### 5.2 基础读写验证

先写，再读：

```sh
/bin/test write hello_day05
/bin/test read
```

预期：

```text
WRITE ok: ...
READ ok: processed: hello_day05
```

说明：

- 写入请求被驱动接收
- workqueue 异步处理后生成输出
- read 从 output_buf 中取到结果

---

### 5.3 阻塞读测试（核心）

由于当前是最小 rootfs + 单串口 shell，前台执行 `read` 后，同一个 shell 不能继续输入第二条命令。
因此推荐使用“后台 read + 前台触发唤醒”的方式验证。

先执行：

```sh
/bin/test read &
```

然后再触发写入或 ioctl：

```sh
/bin/test write wakeup_test
```

或者：

```sh
/bin/test set 123
```

预期：

- 后台的 `read` 会先阻塞
- 约 500ms 后，workqueue 处理完成
- 后台 `read` 被唤醒并输出结果

示例输出：

```text
READ ok: processed: wakeup_test
```

或者：

```text
READ ok: processed: ioctl-set:123
```

这说明：

- `read()` 确实阻塞在 waitqueue
- `write()` / `ioctl(SET)` 能触发异步处理
- `workqueue` 完成后成功唤醒了读者

---

### 5.4 ioctl 测试

```sh
/bin/test set 123
/bin/test get
```

预期：

- `set` 成功
- `get` 输出 `123`

注意：

- `ioctl(SET)` 也会触发 workqueue
- 它主要用于演示 ioctl 路径同样可以参与异步处理

---

### 5.5 sysfs 测试

查看 enable：

```sh
cat /sys/class/demo_class/demo/enable
```

关闭设备：

```sh
echo 0 > /sys/class/demo_class/demo/enable
```

再执行：

```sh
/bin/test write hello
```

预期：

- 返回失败（`EPERM`）

恢复设备：

```sh
echo 1 > /sys/class/demo_class/demo/enable
```

---

### 5.6 debugfs 状态观察

```sh
cat /sys/kernel/debug/demo_debug/status
```

关注这些字段：

- `enable`
- `data_ready`
- `work_pending`
- `value`
- `counter`
- `input_buf`
- `output_buf`

---

## 6. 并发与压测建议

### 6.1 连续写入

```sh
/bin/test write one
/bin/test write two
/bin/test write three
```

预期：

- 如果前一个 work 还未完成，后续写入可能返回 `EBUSY`
- 这是本实验的简化设计：同一时刻只允许一个 pending work

---

### 6.2 连续多次执行

```sh
/bin/test write hello1
/bin/test read

/bin/test write hello2
/bin/test read

/bin/test write hello3
/bin/test read
```

预期：

- 每次都能正常完成
- 无死锁、无卡死、无 Oops

---

### 6.3 观察内核日志

```sh
dmesg | tail -n 50
```

观察是否有：

- warning
- hung task
- Oops
- workqueue 未清理等异常

---

## 7. 验收标准

### 功能验收
- `read()` 在无数据时会阻塞
- `write()` 或 `ioctl(SET)` 会提交异步处理
- workqueue 完成后能唤醒阻塞读
- `sysfs/debugfs` 可正常使用

### 稳定性验收
- 多次读写不崩溃
- 并发测试不死锁
- `rmmod` 前通过 `cancel_work_sync()` 正确清理 work

### 理论验收
能够说清楚：

- waitqueue 的作用
- workqueue 的作用
- 进程上下文 vs workqueue 上下文
- 为什么这里用 mutex 而不是 spinlock

---

## 8. 本日复盘建议

建议记录这些内容：

- 我今天第一次真正理解了什么叫“阻塞读”
- waitqueue 负责“等”，workqueue 负责“异步做”
- read/write/ioctl 都是进程上下文
- workqueue 也是可睡眠上下文
- 退出路径必须 `cancel_work_sync()`，否则可能 UAF


## 9. 本次实际测试记录（已验证）

下面记录的是当前这一版 day05 在 QEMU 中已经跑通的真实测试现象，后续你可以继续按这个格式补充。

### 9.1 设备节点与 sysfs 已就绪

执行：

```sh
ls -l /dev/demo
ls -l /sys/class/demo_class/demo/
```

现象：

- `/dev/demo` 正常创建
- `enable`、`counter` 等 sysfs 属性存在

这说明 day05 的字符设备和 sysfs 控制面已经正常加载。

---

### 9.2 基础 write -> read 路径已跑通

执行：

```sh
/bin/test write hello_day05
/bin/test read
```

现象：

```text
WRITE ok: 11 bytes
READ ok: processed: hello_day05
```

同时内核日志出现：

```text
demo: write accepted, work scheduled
demo: workqueue start
demo: workqueue done, reader woken up
demo: read 22 bytes
```

结论：

- `write()` 成功把请求提交到 workqueue
- workqueue 异步处理完成后生成了输出
- `read()` 能正确读出处理后的结果

---

### 9.3 后台 read + 前台 ioctl 唤醒验证成功

执行：

```sh
/bin/test read &
/bin/test set 123
```

现象：

```text
READ ok: processed: ioctl-set:123
```

结论：

- 后台 `read` 确实先进入等待状态
- 前台 `ioctl(SET)` 触发了 workqueue
- workqueue 完成后通过 `wake_up_interruptible()` 成功唤醒后台 `read`

这就是 day05 最核心的“阻塞等待 -> 异步处理 -> 唤醒读者”验证。

---

### 9.4 关于单串口 shell 的使用说明

当前环境是：

- `console=ttyS0`
- `-nographic`
- busybox 最小 shell

因此会出现两个正常现象：

1. 内核日志可能和命令提示符混在一行显示
2. 如果前台命令阻塞，同一个 shell 里不能继续输入下一条命令

所以 day05 做阻塞测试时，更推荐：

```sh
/bin/test read &
/bin/test write hello
```

或：

```sh
/bin/test read &
/bin/test set 123
```

---

### 9.5 当前版本的结论

当前 day05 已完成以下目标：

- waitqueue 阻塞等待模型已跑通
- workqueue 异步处理模型已跑通
- `write` 与 `ioctl(SET)` 都能作为唤醒触发源
- `read()` 能返回异步处理后的结果
- sysfs/debugfs 基础接口已具备

当前版本已经可以作为 day05 学习基线继续往下推进。
