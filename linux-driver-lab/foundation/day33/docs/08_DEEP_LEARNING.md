# Day33 ftrace 深度学习指南 - function_graph 关键路径解释

## 一、Day33 是什么？

Day33 是 W5 的第五天，承接 Day32 的 perf 热点分析，引入 **ftrace function_graph 动态追踪**。

**核心目标**：对一条固定 workload 采集 `function_graph` 窗口，解释关键调用链与主要耗时点。

Day33 不再追求"新增一个大功能"，而是把前面已经跑通的路径**看清楚、说清楚**。

---

## 二、Day32 → Day33 的核心变化

### 2.1 从"热点测量"到"调用路径解释"

| 维度 | Day32 | Day33 |
|------|-------|-------|
| 工具 | perf stat/record/report | ftrace function_graph |
| 目标 | 找到哪个函数占用 CPU 最多 | 看清函数之间的调用关系和耗时 |
| 输出 | 热点函数列表（flat） | 调用树状图（nested tree） |
| 改进对象 | mmap 复用 | 无（解释已跑通的路径） |

### 2.2 perf vs ftrace

```
perf（Day32）：
  回答"哪个函数最热"（哪个函数占用最多 CPU 时间）
  适合：找到最值得优化的函数

ftrace function_graph（Day33）：
  回答"函数是怎么被调用的、每个函数花了多长时间"
  适合：理解一次操作的完整调用路径
```

---

## 三、ftrace 基础知识

### 3.1 什么是 ftrace？

```
ftrace 是 Linux 内核内置的追踪框架（从 2.6.27 引入）
位于 /sys/kernel/debug/tracing（老路径）或 /sys/kernel/tracing（新路径）

主要功能：
  - function_graph：记录函数调用关系和耗时（##### 调用树 #####）
  - function：记录函数调用（只显示函数名）
  - wakeup_latency：调度延迟追踪
  - irqsoff：中断禁用时间追踪
  - ...（还有很多）
```

### 3.2 function_graph 怎么工作的？

```
工作原理：
  1.在内核函数入口埋桩（probe）
  2.在函数出口埋桩
  3.记录 entry 时间戳、exit 时间戳
  4.计算 duration = exit - entry

输出格式（树状）：
  0)               |      day33_ioctl() {
  0)               |        day33_do_run_dma() {
  0)   0.456 us    |          day33_program_dma() {
  0)   0.123 us    |            day33_wait_dma_idle();
  0)   1.234 us    |          }
  ...
  0)   0.789 us    |        }
  0)               |      }
```

### 3.3 tracefs 路径兼容

```
现代 Linux（4.1+）：/sys/kernel/tracing
老版本 Linux：/sys/kernel/debug/tracing

Day33 guest 脚本会优先探测 /sys/kernel/tracing，
若不可用则回退到 /sys/kernel/debug/tracing。
```

---

## 四、Day33 的 trace workload

### 4.1 为什么选择 mmap-verify？

```
Day33 选择 mmap-verify 作为 trace workload，原因：

1. 路径足够小：
   - 只需一次 ioctl(RUN_DMA)
   - 触发两段 DMA
   - 用户态验证 src/dst

2. 层次清楚：
   - day33_ioctl（入口）
   - day33_do_run_dma（主逻辑）
   - day33_program_dma（DMA 编程，两次）
   - day33_wait_dma_idle（轮询等待）
   - day33_irq_handler（中断完成）

3. 证据容易解释：
   - mmap-verify 已经通过，trace 只是"看清它"
   - 不需要解释"为什么失败"
```

### 4.2 不适合 trace 的 workload

```
Day33 警告：不要 trace 大规模 bench-dma

原因：
  1. trace 本身有开销，会干扰测量结果
  2. 大规模迭代会让 trace 文件巨大（几百 MB）
  3. 噪音过多，难以提取关键路径
  4. 层次太多，解释成本高
```

---

## 五、关键函数路径详解

### 5.1 完整调用链

```
用户态:
    ioctl(RUN_DMA)
        ↓
    syscall: sys_ioctl()
        ↓
    kernel:
    day33_ioctl()         ←── ioctl 入口
        ↓
    day33_do_run_dma()   ←── 主逻辑（两段 DMA）
        ↓
    ┌─ day33_program_dma()   stage 1: RAM→EDU
    │       ↓
    │   day33_wait_dma_idle()   ←── 轮询 DMA_CMD 等待完成
    │       ↓
    │   [IRQ 触发]
    │       ↓
    │   day33_irq_handler()     ←── 中断处理
    └───────┘
        ↓
    ┌─ day33_program_dma()   stage 2: EDU→RAM
    │       ↓
    │   day33_wait_dma_idle()
    │       ↓
    │   [IRQ 触发]
    │       ↓
    │   day33_irq_handler()
    └───────┘
        ↓
    返回 0
        ↓
    用户态 memcmp(src, dst, len)
```

### 5.2 每个函数的作用

| 函数 | 作用 | 典型耗时 |
|------|------|----------|
| `day33_ioctl` | 解析 ioctl 命令，调用 `day33_do_run_dma` | 短（只是分发） |
| `day33_do_run_dma` | 两次 DMA 往返的主逻辑 | ≈ 两段 DMA 总时间 |
| `day33_program_dma` | 写 DMA 寄存器（src/dst/count/cmd） | 微秒级 |
| `day33_wait_dma_idle` | 轮询 DMA_CMD.START 位 | **毫秒级（主成本）** |
| `day33_irq_handler` | 读 IRQ_STATUS，ACK 中断 | 微秒级 |

### 5.3 function_graph 输出示例

```
# tracer: function_graph
#
# CPU  DURATION                  FUNCTION CALLS
# |     |                           |
  0)               |      day33_ioctl() {
  0)               |        day33_do_run_dma() {
  0)   0.521 us    |          day33_program_dma() {
  0)   0.123 us    |            day33_wait_dma_idle();
  0)   0.832 us    |          }
  0)               |          day33_program_dma() {
  0)   0.456 us    |            day33_wait_dma_idle();
  0)   1.234 us    |          }
  0) + 50.123 us   |        }
  0)               |        day33_irq_handler() {
  0)   0.234 us    |        }
  0)               |      }
```

---

## 六、如何读取 function_graph trace

### 6.1 读取顺序

```
1. 找到 day33_ioctl（入口）
2. 向下看它调用了哪些函数
3. 重点关注 day33_do_run_dma
4. 观察两次 day33_program_dma
5. 看 day33_wait_dma_idle 的持续时间
6. 结合 day33_irq_handler 判断设备完成点
```

### 6.2 关键指标

```
DURATION（持续时间）：
  - 函数从 entry 到 exit 的总时间
  - 包括所有子函数调用的时间
  - 单位：微秒（us）或毫秒（ms）

+ 符号：
  - 函数还在执行中（后面有子函数没返回）
  - 在 return 处会显示总时间

子函数耗时 vs 父函数耗时：
  - 如果父函数耗时远大于子函数之和，说明有"缺失时间"
  - 缺失时间通常是：设备等待（轮询/睡眠）
```

### 6.3 与 Day31/Day32 互相印证

```
如果 day33_wait_dma_idle 很显眼：
  → 说明设备等待仍是主成本（QEMU EDU 软件模拟）
  → 与 Day31 bench-dma avg_us ≈ 200ms 一致

如果 day33_ioctl 自身非常短：
  → 说明 syscall 入口不是主瓶颈
  → 与 Day31 bench-ioctl avg_us ≈ 16μs 一致

如果两次 DMA 的 wait 时间差不多：
  → 说明两段 DMA 成本相近（符合预期）
```

---

## 七、ftrace 配置命令

### 7.1 启用 function_graph 的标准流程

```bash
# 1. 确认 tracefs 路径
TRACE_DIR="/sys/kernel/tracing"
[ ! -d "$TRACE_DIR" ] && TRACE_DIR="/sys/kernel/debug/tracing"

# 2. 确认 tracing_on = 1（启用追踪）
echo 1 > $TRACE_DIR/tracing_on

# 3. 设置当前tracer为 function_graph
echo function_graph > $TRACE_DIR/current_tracer

# 4. 设置要 trace 的函数（只 trace day33 相关函数）
echo "day33_ioctl" > $TRACE_DIR/set_graph_function
echo "day33_do_run_dma" >> $TRACE_DIR/set_graph_function
echo "day33_program_dma" >> $TRACE_DIR/set_graph_function
echo "day33_wait_dma_idle" >> $TRACE_DIR/set_graph_function
echo "day33_irq_handler" >> $TRACE_DIR/set_graph_function

# 5. 可选：设置 filter（只 trace 特定进程）
# echo $PID > $TRACE_DIR/set_ftrace_pid

# 6. 运行 workload
./mmap-verify 256 12345

# 7. 关闭追踪
echo 0 > $TRACE_DIR/tracing_on

# 8. 读取 trace
cat $TRACE_DIR/trace
```

### 7.2 常用配置文件

```
tracing_on：控制是否启用追踪（1=启用，0=禁用）
current_tracer：当前使用的 tracer（function_graph 等）
set_graph_function：要 trace 的函数列表
trace：原始 trace 输出
```

---

## 八、tracefs 路径兼容问题

### 8.1 为什么会有路径问题？

```
Linux 内核追踪框架的位置：
  - 老版本（< 4.1）：/sys/kernel/debug/tracing（debugfs）
  - 新版本（>= 4.1）：/sys/kernel/tracing（tracefs）

QEMU 虚拟机的 Linux 版本不同，路径不同。
```

### 8.2 Day33 的处理方式

```
Day33 guest 脚本会：
  1. 优先探测 /sys/kernel/tracing
  2. 若不可写，回退到 /sys/kernel/debug/tracing
  3. 若两者都不可用，不再 panic
  4. 而是在 trace-config.txt 留下失败标记

这样做的好处：
  - 不因为 trace 配置失败导致整个 guest 崩溃
  - 失败信息会沉淀到 records 供归档排障
```

---

## 九、与 Day34 的关系

| 特性 | Day33 | Day34 |
|------|-------|-------|
| 主题 | function_graph 调用路径 | tracepoint 细粒度观测 |
| 工具 | ftrace function_graph | tracepoint + perf |
| 目标 | 看清一条路径的调用关系 | 观测特定事件的详细信息 |
| 输出 | 树状调用图 | 事件点数据 |

---

## 十、验收标准

### 10.1 必须满足

- `mmap-verify` 返回 `verify_ok=1`
- `trace-window.txt` 非空，有有效的 function_graph 输出
- trace 中能识别 `day33_ioctl`、`day33_do_run_dma`、`day33_program_dma`、`day33_wait_dma_idle`
- `irq_delta == 2`（两段 DMA 各触发一次 IRQ）

### 10.2 关键证据

```
trace-window.txt:
  - 能看到 day33_ioctl → day33_do_run_dma 的调用关系
  - 能看到两次 day33_program_dma
  - 能看到 day33_wait_dma_idle 的持续时间

run-result.txt:
  verify_ok=1
  run_ok=1
  irq_delta=2
```

---

## 十一、面试要会讲的五句话

1. **"Day33 的核心是把 Day32 的 perf 热点分析升级为 ftrace 调用路径解释"**
   → Day32 测量哪个函数最热，Day33 看清函数之间的调用关系和耗时

2. **"function_graph 通过在内核函数入口和出口埋桩来记录调用关系"**
   → entry 时记录时间戳，exit 时再记录一次，计算出 duration

3. **"day33_wait_dma_idle 是整个调用链里耗时最长的，因为它在轮询设备"**
   → QEMU EDU 软件模拟，每次轮询 10us，最多轮询 50000 次（500ms 超时）

4. **"tracefs 路径在老版本 Linux 是 debugfs，新版本是 tracefs"**
   → Day33 会自动探测两个路径并回退

5. **"Day33 的 trace workload 选择 mmap-verify 而不是 bench-dma"**
   → 路径小、层次清楚、证据容易解释；大规模 bench 会产生巨大 trace 文件和过多噪音
