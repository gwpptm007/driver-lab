# Day06 - 回归脚本 / 卸载验证 / 并发压力测试


## 0. 先用一句话理解 Day06

Day06 的核心不是“再加一个新接口”，而是开始训练你用**自动化脚本**去验证驱动质量：

- 模块能不能反复装卸，不留下脏状态
- 并发读写时，会不会出现竞态、崩溃、卡死
- 用户态看着没报错时，内核日志里有没有偷偷出现告警

你可以把 Day06 理解成：**从“功能能跑”进入“代码是否稳”**。

---

## 1. 学习目标

今天不再给驱动继续加新接口，而是把 Day05 的能力用于“自动化验证”。

本日目标：

- 把 `insmod/rmmod` 反复执行 500 次，验证模块初始化/卸载路径
- 对 `read/write/ioctl` 做 5 分钟并发压测
- 通过脚本把“可重复执行”的验证沉淀下来
- 重点观察 `dmesg`，确认没有 Oops、UAF、泄漏类告警

本日对应学习计划：

- 任务：回归脚本：insmod/rmmod 500 次；并发读写 5 分钟
- 产出/验收：脚本通过；无 Oops；dmesg 无泄漏告警

---

## 2. Day06 为什么重要

很多驱动在“手点两下功能”时看起来是好的，真正的问题往往出现在：

- 重复加载/卸载以后资源没有清干净
- 有 workqueue 还没收尾就卸载，触发 UAF
- 并发读写下状态机混乱，开始出现偶发性错误
- 用户态测试只覆盖 happy path，没有覆盖 `-EBUSY` / 阻塞等待 / 超时场景

Day06 的重点，就是把这些风险点系统化地测一遍。

---

## 3. 本日代码基线说明

Day06 仍然沿用 Day05 的驱动主体：

- `waitqueue`：阻塞读
- `workqueue`：异步处理写入请求
- `mutex`：保护共享状态
- `sysfs/debugfs`：观察设备状态
- `cancel_work_sync()`：保证卸载路径不会在 worker 尚未退出时释放内存

也就是说，Day06 的新增重点不在内核功能点，而在：

- 用户态测试工具增强
- Guest 内自动化脚本
- 回归记录与验收方法

---

## 4. 新增文件说明

- `insmod_rmmod.sh`
  - 反复执行 `insmod -> set/get/read_timeout -> rmmod`
  - 默认循环 500 次
  - 用来验证模块初始化、设备节点创建、卸载路径和 work 收尾

- `stress_rw.sh`
  - 默认运行 300 秒（5 分钟）
  - 2 个 writer + 2 个 reader 并发运行
  - reader 走 `read_timeout`，避免脚本被永久阻塞
  - writer 允许出现 `-EBUSY`，因为当前驱动是“单槽 pending work”模型

- `check_dmesg.sh`
  - 对 `dmesg` 做关键字扫描
  - 检查 Oops / BUG / KASAN / UAF / leak 等异常痕迹

- `all.sh`
  - 一键串起本日完整验收：
    1. 500 次装卸回归
    2. 再加载模块
    3. 5 分钟并发压测
    4. 最后检查 dmesg

---

## 5. 用户态 test 工具增强点

Day06 的 `test.c` 在 Day05 的基础上增加了两个关键能力：

### 5.1 read_timeout

```bash
/bin/test read_timeout 2
```

作用：

- 最多阻塞 2 秒
- 超时返回 124，而不是让 shell 永久挂住
- 内部使用 `sigaction + alarm`，避免阻塞 `read()` 被不稳定地自动重启

这是 Day06 压测脚本能自动跑起来的关键。

### 5.2 错误码直通退出码

对于 `write/set/read` 失败时，测试工具会尽量把 `errno` 作为退出码返回。

这样脚本就能区分：

- `16`：`-EBUSY`，当前驱动单槽模型下属于预期竞争结果
- `124`：读超时，reader 在压测期间允许出现
- 其他退出码：应视为异常并计入失败

---

## 6. 编译与启动

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day06
chmod +x build.sh
./build.sh
```

进入 QEMU 后，直接执行：

```sh
/bin/all.sh
```

说明：`init` 会为了方便调试先自动加载一次 `demo.ko`。Day06 脚本现在会在正式回归前自动做一次 `rmmod demo`，把环境重置到干净状态，所以不用手工处理。

---

## 7. 分步验证命令

### 7.1 先做 500 次装卸回归

```sh
/bin/insmod_rmmod.sh 500
```

预期：

- 如果模块已被 `init` 预加载，脚本会先自动卸载一次
- 每次 `insmod demo.ko` 成功
- `/dev/demo` 可以创建出来
- `set/get/read_timeout` 可以完成一轮基础功能验证
- 每次 `rmmod demo` 成功
- 脚本最终输出 `PASS`

### 7.2 再做 5 分钟并发读写

先手工加载模块：

```sh
insmod /demo.ko
```

再执行：

```sh
/bin/stress_rw.sh 300
```

预期：

- 2 个 writer 与 2 个 reader 正常退出
- 脚本每 5 秒打印一次进度，便于观察没有“假卡死”
- `busy` 计数允许非 0，因为驱动只有一个 pending work 槽位
- `timeout` 计数允许非 0，因为 reader 是定时阻塞等待
- `err` 计数必须为 0
- 脚本最终输出 `PASS`

### 7.3 检查 dmesg

```sh
/bin/check_dmesg.sh
```

预期：

- 没有命中 Oops / BUG / KASAN / UAF / leak 关键字
- 输出 `PASS: no suspicious dmesg pattern found`

---

## 8. 为什么压测里允许出现 -EBUSY

这是 Day06 很关键的一个“理解点”。

当前驱动在 `write/ioctl(SET)` 路径里采用的是：

- 同一时刻只允许一个 `work_pending`
- 上一个 work 没做完时，新的请求返回 `-EBUSY`

所以在并发压测中，`busy` 不是 bug，而是当前设计下的预期表现。

真正应该当成失败的是：

- `Oops`
- `BUG`
- `use-after-free`
- `memory leak`
- reader / writer 出现脚本未预期的退出码

---

## 9. 推荐验收顺序

```sh
/bin/all.sh
cat /sys/kernel/debug/demo_debug/status
/bin/check_dmesg.sh
```

如果你要记录到测试文档里，建议额外保存：

```sh
dmesg > /tmp/dmesg.txt
cat /sys/kernel/debug/demo_debug/status > /tmp/status.txt
```

---

## 10. 通过标准

本日通过标准建议写成下面这样：

1. `insmod_rmmod.sh 500` 通过
2. `stress_rw.sh 300` 通过
3. `check_dmesg.sh` 未发现异常关键字
4. 手工查看 `dmesg` 无 Oops / Call Trace / leak 类告警
5. `debugfs status` 可读，驱动在压测后仍然保持可观察状态

---

## 11. 下一步

Day06 完成后，Day07 就可以收口 W1：

- 统一 day01~day06 的代码风格
- 收敛 README
- 沉淀一份可以直接放 GitHub/面试讲解的阶段总结


---

## 12. 建议你边看边对照的源码顺序

如果你现在还是初学阶段，建议按下面顺序阅读 Day06：

1. `demo.c`
   - 先看 `struct demo_device`
   - 再看 `demo_write()` / `demo_ioctl()` 怎么把请求变成 `work_pending`
   - 再看 `demo_work_handler()` 怎么异步处理并 `wake_up_interruptible()`
   - 最后看 `demo_read()` 和 `demo_exit()`
2. `test.c`
   - 看用户态怎么触发 `ioctl/read/write`
   - 特别看 `read_timeout`
3. `insmod_rmmod.sh`
   - 理解“为什么要重复装卸 500 次”
4. `stress_rw.sh`
   - 理解“为什么允许 busy/timeout，但不允许其他 err”
5. `check_dmesg.sh`
   - 理解“为什么脚本通过还不够，还要看 dmesg”
6. `all.sh`
   - 最后再看怎么把它们串成一键验收

这样看会比从头到尾平铺读更容易理解。
