# Day34 稳定性测试深度指南 - 并发压测 + 模块循环 + 错误注入

## 一、Day34 是什么？

Day34 是 W5 的倒数第二天，将目标从"功能验证"和"性能分析"转移到**稳定性可回归**。

**核心目标**：回答三个稳定性问题：
1. 多进程并发访问同一设备时是否会误报失败
2. `insmod/rmmod` 循环时资源释放是否完全配对
3. 非法输入是否被明确拒绝

Day34 的意义：前面的 day29-33 证明了功能可用，day34 证明了**在压力下长期运行依然可用**。

---

## 二、为什么需要稳定性测试？

### 2.1 功能 vs 稳定性

| 维度 | Day29-33 | Day34 |
|------|----------|-------|
| 目标 | 证明功能正确 | 证明长期可靠 |
| 方法 | 单次执行验证 | 重复压力验证 |
| 指标 | 功能通过/失败 | 累积成功率 |
| 时间 | 毫秒级 | 分钟~小时级 |

### 2.2 三种失败模式

```
1. 并发失败（竞态）：
   - 多进程同时访问共享缓冲区
   - 结果：数据被踩踏，校验失败

2. 模块循环失败（资源泄漏）：
   - insmod/rmmod 循环时资源未完全释放
   - 结果：rmmod 后 kernel object 残留，insmod 失败

3. 错误注入失败（边界处理缺失）：
   - 非法 len 或 mmap offset 未被拒绝
   - 结果：设备进入半失效状态
```

---

## 三、并发压测设计

### 3.1 为什么需要并发测试？

```
单进程测试：只能证明"我自己没问题"
并发测试：能发现"别人干扰我时的问题"

Day34 并发场景：
  stress-mmap x3：mmap + memcpy + RUN_DMA + memcmp（共享 src/dst）
  stress-ioctl x1：纯 ioctl RUN_DMA（共享 device state）

如果不做协调：
  - 三个 mmap worker 可能同时修改同一个 src 缓冲区
  - 结果：data corruption，但驱动本身没问题
```

### 3.2 用户态协调：flock()

```c
// stress-mmap 协调方式
flock(fd, LOCK_EX);           // 获取排他锁
// 填充 src → RUN_DMA → memcmp(dst, src)
flock(fd, LOCK_UN);           // 释放锁

// 效果：每个 worker 完整执行"填充→DMA→比较"后才轮到下一个
// 驱动看到的是串行请求，不需要驱动自己处理多进程协调
```

### 3.3 驱动端的并发保护

```c
// 驱动使用 op_lock 保护 device state
static int day34_do_run_dma(struct day34_dev *d, u32 len, u32 seed)
{
    mutex_lock(&d->op_lock);      // 互斥锁保护
    // ... DMA 操作 ...
    mutex_unlock(&d->op_lock);
    return ret;
}
```

**注意**：op_lock 保护的是 device state（驱动统计、寄存器状态），不是用户态缓冲区。

---

## 四、模块循环测试

### 4.1 什么是模块循环？

```bash
# Day34 执行 1000 次循环
for i in $(seq 1 1000); do
    rmmod day34_edu_stability
    insmod /root/day34_edu_stability.ko
done
```

### 4.2 资源释放顺序的重要性

```
insmod 时的申请顺序：
  1. pci_enable_device()
  2. dma_set_mask_and_coherent()
  3. dma_alloc_coherent()
  4. pci_alloc_irq_vectors()
  5. request_irq()
  6. cdev_add()
  7. device_create()

rmmod 时的释放顺序（必须相反）：
  1. device_destroy()
  2. cdev_del()
  3. free_irq()              ← 先释放 IRQ
  4. pci_free_irq_vectors() ← 再释放 MSI vectors
  5. dma_free_coherent()    ← 最后释放 DMA
  6. pcim_iounmap_regions()
  7. pci_disable_device()
```

### 4.3 request_irq vs devm_request_irq

```c
// Day34 选择 request_irq() 而不是 devm_request_irq()

// devm_request_irq() 的问题：
//   - 由 devres 管理，释放时机在 remove() 返回之后
//   - 若 remove() 中先 pci_free_irq_vectors()，MSI 被 IRQ 层引用
//   - 可能触发 BUG in drivers/pci/msi.c::free_msi_irqs()

// request_irq() 的好处：
//   - 手动配对，释放时机完全可控
//   - remove() 中可以先 free_irq() 再 pci_free_irq_vectors()
```

```c
// Day34 的正确顺序
static void day34_remove(struct pci_dev *pdev)
{
    struct day34_dev *d = pci_get_drvdata(pdev);

    if (d->irq_requested) {
        free_irq(d->irq_vector, d);    // 第一步：释放 IRQ
        d->irq_requested = false;
    }
    if (d->irq_vectors_allocated) {
        pci_free_irq_vectors(pdev);    // 第二步：释放 MSI vectors
        d->irq_vectors_allocated = false;
    }
    if (d->dma_virt)
        dma_free_coherent(...);        // 第三步：释放 DMA buffer
}
```

### 4.4 irq_vectors_allocated 标志

```c
struct day34_dev {
    // ...
    unsigned int irq_vector;
    bool irq_vectors_allocated;   // 标记 MSI vectors 是否已申请
    bool irq_requested;           // 标记 IRQ handler 是否已注册
    // ...
};

// 为什么要两个标志？
//   - pci_alloc_irq_vectors() 成功不代表 request_irq() 成功
//   - 如果 request_irq() 失败，需要回滚 pci_alloc_irq_vectors()
//   - 如果 remove() 被调用但 probe() 失败，部分资源可能未初始化
//   - 两个标志确保 remove() 只释放真正申请过的资源
```

---

## 五、错误注入测试

### 5.1 为什么要做错误注入？

```
正常测试：输入合法值，验证功能正确
错误注入：输入非法值，验证边界拒绝

错误注入的价值：
  - 证明驱动不会在非法输入下崩溃
  - 证明非法输入不会把设备留在半失效状态
  - 为日后模糊测试（fuzzing）打下基础
```

### 5.2 Day34 的两类错误注入

#### 5.2.1 非法长度（len > max_verify_len）

```c
// 驱动中的检查
static int day34_do_run_dma(struct day34_dev *d, u32 len, u32 seed)
{
    if (!len || len > DAY34_DMA_VERIFY_MAX)  // DAY34_DMA_VERIFY_MAX = 2048
        return -EINVAL;
    // ...
}
```

**测试方法**：
```bash
# 用户态发送 len=3000（大于 2048）
ioctl(fd, RUN_DMA, {len=3000, pattern_seed=0x12345})
# 期望：返回 -EINVAL，驱动不崩溃
```

#### 5.2.2 非法 mmap offset（pgoff != 0）

```c
// 驱动中的检查
static int day34_mmap(struct file *filp, struct vm_area_struct *vma)
{
    if (vma->vm_pgoff != 0) {    // Day34 只支持单页映射，pgoff 必须为 0
        day34_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
        dev_err(&d->pdev->dev, "mmap rejected: pgoff=%lu must be 0", vma->vm_pgoff);
        return -EINVAL;
    }
    // ...
}
```

**测试方法**：
```bash
# mmap 时使用非零 offset
mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 4096);
# 期望：返回 -EINVAL，mmap_ok=0，mmap_error=-22
```

### 5.3 错误结果记录

```c
// day34_record_mmap_result() 记录错误注入结果
static void day34_record_mmap_result(struct day34_dev *d, bool ok, int err,
                                     unsigned long len, unsigned long pgoff)
{
    d->last_mmap_ok = ok ? 1U : 0U;
    d->last_mmap_error = err;      // 负数 errno
    d->last_mmap_len = (u32)len;
    d->last_mmap_pgoff = (u32)pgoff;
}
```

**为什么记录？**
- 错误注入后，用户态通过 `ioctl(GET_RESULT)` 读取验证
- `run-result.txt` 反映的是"最近一次操作"，包括错误注入

---

## 六、压力测试工具链

### 6.1 stress-mmap

```c
// stress-mmap 的工作流程
while (1) {
    flock(fd, LOCK_EX);              // 获取排他锁

    // 1. 通过 mmap 访问 DMA 缓冲区
    src = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    dst = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    // 2. 填充 src 并通过 ioctl 触发 DMA
    memset(src, seed, len);
    ioctl(fd, RUN_DMA, {len, seed});

    // 3. memcmp 验证
    if (memcmp(src, dst, len) != 0) {
        printf("VERIFY FAILED!\n");
        fail_count++;
    }

    munmap(src, 4096);
    munmap(dst, 4096);
    flock(fd, LOCK_UN);

    iteration++;
}
```

### 6.2 stress-ioctl

```c
// stress-ioctl 的工作流程（不需要 mmap）
while (1) {
    ioctl(fd, RUN_DMA, {len, seed});  // 直接通过 ioctl 触发 DMA

    // 不做 memcmp，因为没有 mmap 访问 DMA buffer
    // 只验证 ioctl 返回值

    iteration++;
}
```

### 6.3 模块循环脚本

```bash
# 1000 次 insmod/rmmod 循环
for i in $(seq 1 1000); do
    rmmod day34_edu_stability
    insmod /root/day34_edu_stability.ko

    if [ $? -ne 0 ]; then
        echo "Loop $i: insmod failed"
        break
    fi

    if [ $((i % 100)) -eq 0 ]; then
        echo "Completed $i loops"
    fi
done

echo "Module loop completed: $i iterations"
```

---

## 七、与 Day33 的关系

### 7.1 继承关系

```
Day33：ftrace function_graph 看清调用路径
Day34：在 Day33 基础上做压力测试

Day33 的基线能力（Day34 继承）：
  - coherent DMA + mmap + RUN_DMA 数据路径
  - ioctl GET_INFO / RUN_DMA / GET_RESULT 接口
  - PCI/MSI/IRQ 中断处理
  - 统计计数器（total_run_calls/ok/fail）
```

### 7.2 核心区别

| 维度 | Day33 | Day34 |
|------|-------|-------|
| 目标 | 看清调用路径 | 验证长期可靠 |
| 方法 | ftrace 追踪 | 压力测试 |
| 关注点 | 函数耗时 | 资源泄漏/竞态 |
| 输出 | trace 树状图 | 成功率统计 |

---

## 八、Day34 的创新点

### 8.1 手动 IRQ 管理

```c
// 不是 devm_request_irq()，而是 request_irq() + free_irq()
ret = request_irq(d->irq_vector, day34_irq_handler, 0, DAY34_DRV_NAME, d);
if (ret)
    goto err_irq_vectors;
d->irq_requested = true;

// remove() 中手动释放
if (d->irq_requested) {
    free_irq(d->irq_vector, d);
    d->irq_requested = false;
}
if (d->irq_vectors_allocated) {
    pci_free_irq_vectors(pdev);
    d->irq_vectors_allocated = false;
}
```

### 8.2 双标志资源追踪

```c
// irq_vectors_allocated：MSI vectors 是否已申请
// irq_requested：IRQ handler 是否已注册

// 为什么需要两个？
//   - pci_alloc_irq_vectors() 成功 → irq_vectors_allocated = true
//   - request_irq() 成功 → irq_requested = true
//   - request_irq() 失败 → 需要回滚 pci_alloc_irq_vectors()
//   - remove() 被调用但 probe() 部分失败 → 只释放已申请的资源
```

### 8.3 mmap 结果快照

```c
// 驱动记录"最近一次 mmap 尝试"的结果
// 用于：错误注入验证、调试、回归分析

struct day34_run_result {
    __u32 mmap_ok;       // 最近一次 mmap 是否成功
    __s32 mmap_error;   // errno（负数）
    __u32 mmap_len;      // mmap 请求的 length
    __u32 mmap_pgoff;    // mmap 请求的 pgoff
};
```

---

## 九、常见问题

### 9.1 run-result.txt 里的计数为什么是 0？

**答**：这是**预期现象**。

Day34 的执行顺序：
1. `mmap-verify`（正常 DMA）
2. `concurrent-stress`（并发压测）
3. `module-loop`（1000 次循环）
4. `fault-invalid-len`（错误注入）
5. `fault-mmap-offset`（错误注入）
6. `result`（读取最近一次操作状态）

`result` 读取的是"最近一次操作"，即 `fault-mmap-offset`：
- `mmap_ok=0`，`mmap_error=-22`（EINVAL）

**真正的验收依据**：
- `mmap-verify.txt` → 主数据路径可用
- `concurrent-stress.txt` → 并发通过
- `module-loop.txt` → 1000 次循环通过
- `fault-invalid-len.txt` → 错误拒绝
- `fault-mmap-offset.txt` → 错误拒绝

### 9.2 为什么 stress-mmap 需要 flock？

**答**：保护用户态共享缓冲区。

Day34 的 DMA 缓冲区布局：
```
dma_virt[0...2047]     → src
dma_virt[2048...4095]  → dst
```

三个 `stress-mmap` worker 共享同一个 `/dev/day34_edu0`：
- 如果不做协调，worker A 填充 src 时，worker B 可能正在读取 src
- 结果：data corruption，但驱动本身没问题
- `flock()` 保证每个 worker 完整执行"填充→DMA→比较"后才让给下一个

### 9.3 mmap offset 为什么必须是 0？

**答**：Day34 只映射一页（4096 bytes），不支持分页。

```
vma->vm_pgoff = 0 时：映射整个 DMA buffer（4096 bytes）
vma->vm_pgoff = 1 时：偏移 4096 bytes，超出 DMA buffer 范围

如果允许非零 offset：
  - 用户可以映射超出 DMA buffer 的区域
  - 导致未定义行为

所以 Day34 只接受 vm_pgoff == 0。
```

---

## 十、面试要会讲的五句话

1. **"Day34 的核心是把 day33 的功能基线转化为稳定性可回归"**
   → 通过并发压测、模块循环、错误注入验证长期可靠性

2. **"模块循环测试的是资源释放顺序，特别是 request_irq() vs pci_free_irq_vectors()"**
   → devm_request_irq() 的释放时机不可控，手动配对是稳定性测试的基础

3. **"双标志 irq_vectors_allocated 和 irq_requested 确保部分失败时只释放已申请的资源"**
   → probe 失败回滚、remove 阶段资源释放都需要精确追踪

4. **"flock() 在用户态协调并发访问，避免把用户态竞态误报为驱动问题"**
   → 驱动使用 op_lock 保护 device state，用户态用 flock 保护自己的缓冲区

5. **"run-result.txt 的 mmap_ok=0 是预期现象，因为它反映的是最后一次错误注入"**
   → 真正的验收依据是各个独立的 test record，不是 result 快照

---

## 十一、验收标准

### 11.1 必须满足

- `mmap-verify.txt`：`verify_ok=1`
- `concurrent-stress.txt`：所有 worker `rc=0`，`worker_fail=0`
- `module-loop.txt`：`completed_loops=1000`，`failed_loops=0`
- `fault-invalid-len.txt`：`mmap_ok=1`（ioctl 成功）或返回 `-EINVAL`
- `fault-mmap-offset.txt`：`mmap_ok=0`，`mmap_error=-22`
- guest 正常结束，无 `BUG/Oops/panic`

### 11.2 关键证据

```
mmap-verify.txt：
  verify_ok=1
  run_ok=1

concurrent-stress.txt：
  worker_fail=0 (所有 4 个 worker)

module-loop.txt：
  completed_loops=1000
  failed_loops=0

fault-mmap-offset.txt：
  mmap_ok=0
  mmap_error=-22
```
