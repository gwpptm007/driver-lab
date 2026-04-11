#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/irqflags.h>
#include <linux/atomic.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/of_device.h>
#include <linux/version.h>

#define DRV_NAME             "demo_regmap"
#define DEMO_DEBUGFS_DIR     DRV_NAME
#define DEMO_VERSION_CODE    0x00001300


/*
 * Day18 复用说明
 * ---------------
 * Day18 的目标不是再造一个新的教学驱动，而是把 day15/day16 拆散的 baseline、
 * 裁剪、采样、文档全部重新收口成一个独立实验目录。
 *
 * 因此这里继续复用 demo_regmap.c：
 * - 驱动功能本身仍然是同一个教学对象；
 * - 但构建、rootfs、QEMU、采样、裁剪 profile、结果归档都迁移到 day18/；
 * - 你后续学习时，可以把这个驱动当成“固定观测目标”，把更多精力放在
 *   W3 的工程化主线上。
 */

/*
 * Day13 的教学目标
 * ----------------
 * 这一版驱动继续复用 day12 的“software regmap + debugfs + IRQ + workqueue”骨架，
 * 但重点从“功能做出来”转向“调用路径看明白”：
 *
 *   trigger(write)
 *     -> generic_handle_irq()
 *     -> handle_fasteoi_irq()
 *     -> handle_irq_event()
 *     -> __handle_irq_event_percpu()
 *     -> demo_regmap_handler()          // top-half
 *     -> queue_work()
 *     -> worker_thread/process_one_work
 *     -> demo_regmap_workfn()           // bottom-half
 *
 * 为什么继续使用 day12 的 demo_regmap：
 * 1. trigger/snapshot/poke 三个入口已经稳定，适合重复实验；
 * 2. regmap + debugfs 已经把状态组织成“寄存器视图”，便于 trace 前后对照；
 * 3. day13 只需要关注“谁调用了谁、哪个上下文执行、哪段更耗时”。
 *
 * Day13 最重要的理解点：
 * - top-half 仍然应该极短，只做“抢现场”；
 * - bottom-half(workqueue) 才承担可睡眠的重活；
 * - function_graph 追踪的是“函数调用图”，因此非常适合观察这条路径。
 */
static unsigned int default_work_ms = 20;
module_param(default_work_ms, uint, 0644);
MODULE_PARM_DESC(default_work_ms,
                 "default simulated heavy work duration in milliseconds");

/*
 * 教学寄存器地图
 * --------------
 * 这里并没有真的去访问一段外设 MMIO，而是用 shadow regs[] + regmap 封装出一组
 * “像寄存器一样可读写”的状态视图。这么做的目的有两层：
 *
 * 1. 学 regmap 的抽象思想
 *    先学会“统一寄存器访问入口”，而不是一开始就依赖真实硬件。
 *
 * 2. 学状态建模
 *    把 day11 的 irq_count/work_runs/latency 等运行态整理成固定 offset 的寄存器，
 *    这样 snapshot/debugfs/read/write 都能围绕同一套模型组织。
 *
 * 这些 offset 都按 4 字节步进，方便模拟最常见的 32-bit 寄存器布局。
 */
#define DEMO_REG_CTRL             0x00 /* 控制寄存器：bit0=enable，是否允许 trigger 触发 fake IRQ */
#define DEMO_REG_STATUS           0x04 /* 状态寄存器：当前 enable / pending / busy 状态组合 */
#define DEMO_REG_IRQ_COUNT        0x08 /* top-half 总触发次数：每进一次 handler 加 1 */
#define DEMO_REG_WORK_RUNS        0x0c /* worker 实际运行次数：一次 batch 记一次 */
#define DEMO_REG_WORK_ITEMS       0x10 /* worker 实际处理的事件总数：可与 irq_count 对照 */
#define DEMO_REG_PENDING_EVENTS   0x14 /* 当前待处理事件数：top-half 加，worker 用 atomic_xchg 取走 */
#define DEMO_REG_LAST_BATCH       0x18 /* 最近一轮 worker 一次性处理了多少个 pending */
#define DEMO_REG_LAST_LATENCY_US  0x1c /* 最近一次粗略延迟(微秒)：首个 pending IRQ -> worker 开始 */
#define DEMO_REG_MAX_LATENCY_US   0x20 /* 历史最大粗略延迟(微秒) */
#define DEMO_REG_AVG_LATENCY_US   0x24 /* 平均粗略延迟(微秒) */
#define DEMO_REG_WORK_MS          0x28 /* worker 模拟重活时长(毫秒)，可通过 poke 改写 */
#define DEMO_REG_VERSION          0x2c /* 版本号：帮助区分 day12/day13 的教学驱动演进 */

#define DEMO_REG_STRIDE           4
#define DEMO_MAX_REGISTER         DEMO_REG_VERSION
#define DEMO_REG_COUNT            ((DEMO_MAX_REGISTER / DEMO_REG_STRIDE) + 1)

/* CTRL 的 bit 定义 */
#define DEMO_CTRL_ENABLE          BIT(0)
#define DEMO_CTRL_WRITABLE_MASK   DEMO_CTRL_ENABLE

/* STATUS 的 bit 定义 */
#define DEMO_STATUS_PENDING       BIT(0) /* 还有 pending 事件没被 worker 取走 */
#define DEMO_STATUS_WORK_BUSY     BIT(1) /* worker 正在执行 demo_regmap_workfn() */
#define DEMO_STATUS_ENABLED       BIT(2) /* CTRL.enable 生效中，允许 trigger 触发 */

/*
 * 驱动私有结构体
 * ---------------
 * 这份结构体把 day09/day10/day11/day12/day13 的关键知识串起来了：
 *
 * 1. platform/DT 资源解析
 *    - dev / mem / linux_irq / raw_reg / raw_irq / label / match_name
 *
 * 2. IRQ top-half 与 bottom-half
 *    - irq_count / pending_events / first_pending_irq_ns / last_irq_ns
 *    - work / wq / work_runs / work_items / work_busy / last_batch
 *
 * 3. 延迟统计
 *    - last/max/sum/samples
 *
 * 4. regmap + debugfs
 *    - regmap / reg_lock / regs[]
 *    - snapshot / poke / trigger
 *
 * 建议你读代码时把它当成“整份驱动的地图”：
 * 看懂每个字段从哪里更新、在哪里被读出，后面的函数就容易串起来。
 */
struct demo_regmap_priv {
    struct device *dev;                  /* 设备对象：日志打印、devm 资源管理、debug 入口都依赖它 */
    struct resource mem;                 /* DT 解析出的 MEM 资源；day13 仍保留这条教学链路 */
    int linux_irq;                       /* Linux 虚拟 IRQ 号：request_irq()/generic_handle_irq() 用它 */

    u32 raw_reg[4];                      /* 原始 reg cells：帮助理解 DT 的 <addr size> 解析结果 */
    u32 raw_irq[3];                      /* 原始 interrupts cells：帮助理解 GIC 三元组 */
    const char *label;                   /* 设备标签：来自 demo,label，便于 README/日志解释 */
    const char *match_name;              /* compatible 命中值：说明 of_match_table 的匹配结果 */

    atomic64_t irq_count;                /* top-half 被触发的总次数 */
    atomic64_t work_runs;                /* worker 实际运行次数：一次 batch 算一次 */
    atomic64_t work_items;               /* worker 处理过的事件总数：通常等于累计 pending */

    atomic_t pending_events;             /* 当前待处理事件数：中断来时增加，worker 取走后归零 */
    atomic_t last_batch;                 /* 最近一次 worker 一次处理多少个 pending */
    atomic_t work_busy;                  /* worker 当前是否正在干活；用于 STATUS.busy */

    atomic64_t first_pending_irq_ns;     /* 当前 batch 中第一个 IRQ 时间戳：用于算批处理等待延迟 */
    atomic64_t last_irq_ns;              /* 最近一次 IRQ 时间戳：帮助理解“最近触发”和“当前 batch 首次触发”的差别 */

    atomic64_t last_latency_ns;          /* 最近一次测得的粗略延迟 */
    atomic64_t max_latency_ns;           /* 历史最大粗略延迟 */
    atomic64_t sum_latency_ns;           /* 延迟累计值：配合 latency_samples 计算平均值 */
    atomic64_t latency_samples;          /* 延迟样本数 */

    struct workqueue_struct *wq;         /* 私有 ordered workqueue：让 bottom-half 顺序执行，输出更稳 */
    struct work_struct work;             /* 唯一的 work 对象：体现同一 work_struct 会被合并调度 */
    unsigned int work_delay_ms;          /* worker 模拟重活时长：可通过 poke 改，便于看 trace 时长差异 */

    struct regmap *regmap;               /* regmap 句柄：统一寄存器读写 API，不直接暴露 regs[] */
    struct mutex reg_lock;               /* 保护 shadow regs[] 的锁：regmap 读写和视图刷新都要用 */
    u32 regs[DEMO_REG_COUNT];            /* 教学用 shadow register 数组：本质上是“软件寄存器后端” */

    struct dentry *debugfs_dir;          /* /sys/kernel/debug/demo_regmap 根目录 */
    struct dentry *debugfs_snapshot;     /* snapshot：只读，导出寄存器快照 */
    struct dentry *debugfs_poke;         /* poke：只写，验证 regmap_write() 路径 */
    struct dentry *debugfs_trigger;      /* trigger：只写，软件触发 fake IRQ */
};

/*
 * demo_regmap_dump_raw_reg()
 * --------------------------
 * 读取设备树中的 reg 属性原始值，并直接打印出来。
 *
 * 为什么不只依赖 platform_get_resource()？
 * 因为 day09/day10/day11/day12/day13 一直有个教学目标：
 * “不仅知道内核帮你解析后的结果，也要看到设备树里最原始的 cells 长什么样”。
 *
 * 对于 64-bit address/size 的 virt 设备树，经常能看到：
 *   <0x0 0x10002000 0x0 0x1000>
 * 这比只看最终 start/end 更能帮助建立 DT 直觉。
 */
static void demo_regmap_dump_raw_reg(struct device *dev,
                                     struct device_node *np,
                                     struct demo_regmap_priv *priv)
{
    int ret;

    ret = of_property_read_u32_array(np, "reg", priv->raw_reg,
                                     ARRAY_SIZE(priv->raw_reg));
    if (ret) {
        dev_warn(dev, "raw DT reg read failed: %d\n", ret);
        return;
    }

    dev_info(dev, "raw DT reg cells: <%#x %#x %#x %#x>\n",
             priv->raw_reg[0], priv->raw_reg[1],
             priv->raw_reg[2], priv->raw_reg[3]);
}

/*
 * demo_regmap_dump_raw_irq()
 * --------------------------
 * 读取设备树中的 interrupts 原始 cells。
 *
 * 你在 day10/day12 里看到过：
 *   <0x0 0x7a 0x4>
 *
 * 对 GIC 来说，它通常表示：
 * - 0x0 : SPI/PPI 类型中的一个类别值
 * - 0x7a: 中断号 122
 * - 0x4 : 触发类型(level-high)
 *
 * 真正给驱动用的是 platform_get_irq() 得到的 Linux IRQ 号；
 * 原始 raw_irq 更适合教学观察和 README 留档。
 */
static void demo_regmap_dump_raw_irq(struct device *dev,
                                     struct device_node *np,
                                     struct demo_regmap_priv *priv)
{
    int ret;

    ret = of_property_read_u32_array(np, "interrupts", priv->raw_irq,
                                     ARRAY_SIZE(priv->raw_irq));
    if (ret) {
        dev_warn(dev, "raw DT interrupts read failed: %d\n", ret);
        return;
    }

    dev_info(dev, "raw DT interrupts cells: <%#x %#x %#x>\n",
             priv->raw_irq[0], priv->raw_irq[1], priv->raw_irq[2]);
}

/*
 * demo_regmap_update_latency()
 * ----------------------------
 * 更新粗略延迟统计：最近值 / 累计和 / 样本数 / 最大值。
 *
 * 为什么这里用 atomic64_cmpxchg() 更新最大值？
 * 因为 max_latency_ns 是一个共享统计量，worker 未来如果扩展成并发场景，
 * 直接“先读再写”可能有竞争；cmpxchg 的写法更通用，也更像内核统计代码的习惯写法。
 */
static void demo_regmap_update_latency(struct demo_regmap_priv *priv, u64 latency_ns)
{
    u64 max_ns;

    atomic64_set(&priv->last_latency_ns, latency_ns);
    atomic64_add(latency_ns, &priv->sum_latency_ns);
    atomic64_inc(&priv->latency_samples);

    max_ns = atomic64_read(&priv->max_latency_ns);
    while (latency_ns > max_ns) {
        if (atomic64_cmpxchg(&priv->max_latency_ns, max_ns, latency_ns) == max_ns)
            break;
        max_ns = atomic64_read(&priv->max_latency_ns);
    }
}

/*
 * demo_regmap_refresh_view()
 * --------------------------
 * 把运行态原子变量同步成 shadow regs[]。
 *
 * 这是 day12/day13 中“状态视图”和“访问 API”解耦的关键函数：
 * - top-half / worker 更新的是原子统计量；
 * - snapshot 读取时，不直接逐个读原子量，而是先刷新成寄存器视图；
 * - 然后再通过 regmap_read() 一项项读出来。
 *
 * 这样的好处：
 * 1. debugfs snapshot 和未来其他输出方式都能共享同一套寄存器模型；
 * 2. 你能更清楚地区分“内部状态源”和“对外寄存器呈现”。
 */
static void demo_regmap_refresh_view(struct demo_regmap_priv *priv)
{
    u32 ctrl;
    u32 status = 0;
    u64 avg_ns = 0;
    u64 samples;
    u64 sum_ns;

    mutex_lock(&priv->reg_lock);

    ctrl = priv->regs[DEMO_REG_CTRL / DEMO_REG_STRIDE];
    if (ctrl & DEMO_CTRL_ENABLE)
        status |= DEMO_STATUS_ENABLED;
    if (atomic_read(&priv->pending_events) > 0)
        status |= DEMO_STATUS_PENDING;
    if (atomic_read(&priv->work_busy))
        status |= DEMO_STATUS_WORK_BUSY;

    priv->regs[DEMO_REG_STATUS / DEMO_REG_STRIDE] = status;
    priv->regs[DEMO_REG_IRQ_COUNT / DEMO_REG_STRIDE] =
        (u32)atomic64_read(&priv->irq_count);
    priv->regs[DEMO_REG_WORK_RUNS / DEMO_REG_STRIDE] =
        (u32)atomic64_read(&priv->work_runs);
    priv->regs[DEMO_REG_WORK_ITEMS / DEMO_REG_STRIDE] =
        (u32)atomic64_read(&priv->work_items);
    priv->regs[DEMO_REG_PENDING_EVENTS / DEMO_REG_STRIDE] =
        (u32)atomic_read(&priv->pending_events);
    priv->regs[DEMO_REG_LAST_BATCH / DEMO_REG_STRIDE] =
        (u32)atomic_read(&priv->last_batch);
    priv->regs[DEMO_REG_LAST_LATENCY_US / DEMO_REG_STRIDE] =
        (u32)(atomic64_read(&priv->last_latency_ns) / 1000ULL);
    priv->regs[DEMO_REG_MAX_LATENCY_US / DEMO_REG_STRIDE] =
        (u32)(atomic64_read(&priv->max_latency_ns) / 1000ULL);

    samples = atomic64_read(&priv->latency_samples);
    sum_ns = atomic64_read(&priv->sum_latency_ns);
    if (samples)
        avg_ns = div64_u64(sum_ns, samples);

    priv->regs[DEMO_REG_AVG_LATENCY_US / DEMO_REG_STRIDE] = (u32)(avg_ns / 1000ULL);
    priv->regs[DEMO_REG_WORK_MS / DEMO_REG_STRIDE] = priv->work_delay_ms;
    priv->regs[DEMO_REG_VERSION / DEMO_REG_STRIDE] = DEMO_VERSION_CODE;

    mutex_unlock(&priv->reg_lock);
}

/*
 * demo_regmap_is_valid_reg()
 * --------------------------
 * regmap 会把寄存器 offset 作为“地址”来访问。这个 helper 负责做最基本的边界和对齐检查：
 * - 不能超过 max_register
 * - 必须按 4-byte 步进对齐
 */
static bool demo_regmap_is_valid_reg(unsigned int reg)
{
    if (reg > DEMO_MAX_REGISTER)
        return false;
    if (reg % DEMO_REG_STRIDE)
        return false;
    return true;
}

/*
 * demo_regmap_writeable_reg()/readable_reg()/volatile_reg()
 * --------------------------------------------------------
 * 这三类回调是 regmap_config 的重要组成部分：
 *
 * - readable_reg : 这个 offset 是否允许被 regmap_read()
 * - writeable_reg: 这个 offset 是否允许被 regmap_write()
 * - volatile_reg : 这个寄存器是不是“值经常变化”，不要假设缓存稳定
 *
 * 在真实驱动里，这些回调常用来告诉 regmap：
 * 哪些寄存器可写、哪些寄存器只读、哪些寄存器每次都应该读新值。
 */
static bool demo_regmap_writeable_reg(struct device *dev, unsigned int reg)
{
    switch (reg) {
    case DEMO_REG_CTRL:
    case DEMO_REG_WORK_MS:
        return true;
    default:
        return false;
    }
}

static bool demo_regmap_readable_reg(struct device *dev, unsigned int reg)
{
    return demo_regmap_is_valid_reg(reg);
}

static bool demo_regmap_volatile_reg(struct device *dev, unsigned int reg)
{
    switch (reg) {
    case DEMO_REG_STATUS:
    case DEMO_REG_IRQ_COUNT:
    case DEMO_REG_WORK_RUNS:
    case DEMO_REG_WORK_ITEMS:
    case DEMO_REG_PENDING_EVENTS:
    case DEMO_REG_LAST_BATCH:
    case DEMO_REG_LAST_LATENCY_US:
    case DEMO_REG_MAX_LATENCY_US:
    case DEMO_REG_AVG_LATENCY_US:
        return true;
    default:
        return false;
    }
}

/*
 * demo_regmap_reg_read()
 * ----------------------
 * 这是 regmap 的“读后端”。
 *
 * 在 I2C/SPI/MMIO 场景里，这里通常会走总线或寄存器读操作；
 * 本教学驱动里，为了不依赖真实硬件，直接从 shadow regs[] 取值。
 *
 * 你可以把它理解成：
 *   regmap_read() -> demo_regmap_reg_read() -> regs[]
 */
static int demo_regmap_reg_read(void *context, unsigned int reg, unsigned int *val)
{
    struct demo_regmap_priv *priv = context;

    if (!demo_regmap_is_valid_reg(reg))
        return -EINVAL;

    mutex_lock(&priv->reg_lock);
    *val = priv->regs[reg / DEMO_REG_STRIDE];
    mutex_unlock(&priv->reg_lock);

    return 0;
}

/*
 * demo_regmap_reg_write()
 * -----------------------
 * 这是 regmap 的“写后端”。
 *
 * 这里只允许改写：
 * - CTRL    : 用于 enable/disable fake trigger
 * - WORK_MS : 用于控制 worker 模拟重活时长
 *
 * 其他寄存器一律视为只读。
 *
 * 这里顺手演示了一个常见设计点：
 * “对外暴露一个配置寄存器，内部则顺带更新真正驱动用到的成员变量”。
 * 例如写 WORK_MS 时，不只是改 regs[]，也会同步更新 priv->work_delay_ms。
 */
static int demo_regmap_reg_write(void *context, unsigned int reg, unsigned int val)
{
    struct demo_regmap_priv *priv = context;

    if (!demo_regmap_is_valid_reg(reg))
        return -EINVAL;

    mutex_lock(&priv->reg_lock);

    switch (reg) {
    case DEMO_REG_CTRL:
        priv->regs[reg / DEMO_REG_STRIDE] = val & DEMO_CTRL_WRITABLE_MASK;
        break;
    case DEMO_REG_WORK_MS:
        if (val > 5000)
            val = 5000;                 /* 给教学实验加个上限，避免误写成超长 sleep */
        priv->work_delay_ms = val;
        priv->regs[reg / DEMO_REG_STRIDE] = priv->work_delay_ms;
        break;
    default:
        mutex_unlock(&priv->reg_lock);
        return -EACCES;
    }

    mutex_unlock(&priv->reg_lock);
    return 0;
}

/*
 * regmap_config
 * -------------
 * 这是 regmap 的核心配置表，等价于告诉内核：
 * “我的寄存器长什么样，哪些能读写，实际读写要调用哪个后端”。
 *
 * 这里最值得记住的字段：
 * - reg_bits / val_bits : 寄存器地址位宽、数据位宽
 * - reg_stride          : 寄存器步进
 * - max_register        : 最大合法寄存器 offset
 * - readable/writeable/volatile_reg
 * - reg_read / reg_write: 真正的后端回调
 * - can_sleep           : 后端读写允许睡眠；软件后端当然没问题
 * - cache_type          : 这里禁用 regcache，避免教学时引入额外缓存层理解成本
 */
static const struct regmap_config demo_regmap_config = {
    .name = DRV_NAME,
    .reg_bits = 32,
    .val_bits = 32,
    .reg_stride = DEMO_REG_STRIDE,
    .max_register = DEMO_MAX_REGISTER,
    .readable_reg = demo_regmap_readable_reg,
    .writeable_reg = demo_regmap_writeable_reg,
    .volatile_reg = demo_regmap_volatile_reg,
    .reg_read = demo_regmap_reg_read,
    .reg_write = demo_regmap_reg_write,
    .can_sleep = true,
    .fast_io = false,
    .cache_type = REGCACHE_NONE,
};

/*
 * demo_regmap_workfn()
 * --------------------
 * 这是 day11/day12/day13 的 bottom-half，也就是 workqueue 真正执行“重活”的地方。
 *
 * 核心步骤：
 * 1. 用 atomic_xchg(&pending_events, 0) 一次性取走当前 batch；
 * 2. 标记 work_busy，并更新 work_runs/work_items/last_batch；
 * 3. 计算“首个 pending IRQ 到 worker 开始执行”的粗略延迟；
 * 4. 记录日志；
 * 5. msleep(work_delay_ms) 模拟耗时处理。
 *
 * 为什么这里用 for (;;) 循环？
 * 因为 worker 跑起来后，可能在处理一批事件的同时又有新的 IRQ 到来，
 * 这些新的 pending 事件会再次累积。循环可以让同一轮 worker 继续“吃干净”新的 batch。
 *
 * 为什么 batch 处理在教学上很重要？
 * 因为它能解释一个常见现象：
 *   IRQ_COUNT 可能是 5，但 WORK_RUNS 只有 2。
 * 这不是错，而是因为同一个 work_struct 会被合并调度。
 */
static void demo_regmap_workfn(struct work_struct *work)
{
    struct demo_regmap_priv *priv;

    priv = container_of(work, struct demo_regmap_priv, work);

    for (;;) {
        int batch;
        u64 work_start_ns;
        u64 first_ns;
        u64 latency_ns;

        batch = atomic_xchg(&priv->pending_events, 0);
        if (batch <= 0)
            break;

        atomic_set(&priv->work_busy, 1);
        atomic_set(&priv->last_batch, batch);
        atomic64_inc(&priv->work_runs);
        atomic64_add(batch, &priv->work_items);

        work_start_ns = ktime_get_ns();
        first_ns = atomic64_read(&priv->first_pending_irq_ns);
        if (work_start_ns >= first_ns)
            latency_ns = work_start_ns - first_ns;
        else
            latency_ns = 0;

        demo_regmap_update_latency(priv, latency_ns);

        dev_info_ratelimited(priv->dev,
                             "worker start batch=%d latency=%llu us pending_now=%d work_ms=%u\n",
                             batch,
                             (unsigned long long)(latency_ns / 1000ULL),
                             atomic_read(&priv->pending_events),
                             priv->work_delay_ms);

        if (priv->work_delay_ms)
            msleep(priv->work_delay_ms); /* workqueue 运行在 process context，因此允许 sleep */

        atomic_set(&priv->work_busy, 0);
    }
}

/*
 * demo_regmap_handler()
 * ---------------------
 * 这是 top-half，也就是 request_irq() 注册的中断处理函数。
 *
 * 它故意保持极短，只做：
 * - 记录当前 IRQ 时间戳
 * - irq_count++
 * - pending_events++
 * - 如果这是当前 batch 的第一个 pending，就记录 first_pending_irq_ns
 * - queue_work() 把后续处理交给 workqueue
 * - 返回 IRQ_HANDLED
 *
 * 这里千万不要做的事：
 * - 大循环
 * - 长时间打印
 * - 复杂字符串处理
 * - 任何可睡眠操作
 *
 * 这正是 day11“top-half 只抢现场”的核心原则。
 */
static irqreturn_t demo_regmap_handler(int irq, void *dev_id)
{
    struct demo_regmap_priv *priv = dev_id;
    u64 now_ns;
    s64 new_count;
    int pending;

    now_ns = ktime_get_ns();
    new_count = atomic64_inc_return(&priv->irq_count);
    atomic64_set(&priv->last_irq_ns, now_ns);

    pending = atomic_inc_return(&priv->pending_events);
    if (pending == 1)
        atomic64_set(&priv->first_pending_irq_ns, now_ns);

    queue_work(priv->wq, &priv->work);

    dev_info_ratelimited(priv->dev,
                         "top-half irq=%d irq_count=%lld pending=%d\n",
                         irq, (long long)new_count, pending);

    return IRQ_HANDLED;
}

/*
 * demo_regmap_reg_desc[]
 * ----------------------
 * 给 snapshot 输出准备一个“寄存器描述表”，把 offset 与可读名字绑定起来。
 *
 * 这种写法的好处：
 * - 输出格式统一；
 * - 后面新增寄存器时，只要加表项，不用散落修改打印逻辑。
 */
static const struct {
    unsigned int reg;
    const char *name;
} demo_regmap_reg_desc[] = {
    { DEMO_REG_CTRL,            "CTRL" },
    { DEMO_REG_STATUS,          "STATUS" },
    { DEMO_REG_IRQ_COUNT,       "IRQ_COUNT" },
    { DEMO_REG_WORK_RUNS,       "WORK_RUNS" },
    { DEMO_REG_WORK_ITEMS,      "WORK_ITEMS" },
    { DEMO_REG_PENDING_EVENTS,  "PENDING_EVENTS" },
    { DEMO_REG_LAST_BATCH,      "LAST_BATCH" },
    { DEMO_REG_LAST_LATENCY_US, "LAST_LATENCY_US" },
    { DEMO_REG_MAX_LATENCY_US,  "MAX_LATENCY_US" },
    { DEMO_REG_AVG_LATENCY_US,  "AVG_LATENCY_US" },
    { DEMO_REG_WORK_MS,         "WORK_MS" },
    { DEMO_REG_VERSION,         "VERSION" },
};

/*
 * demo_regmap_snapshot_show()
 * ---------------------------
 * snapshot 的 seq_file show 回调。
 *
 * 这条路径是 day12/day13 验证 regmap 读路径最直接的地方：
 * 1. 先调用 demo_regmap_refresh_view() 把原子统计量刷新到 shadow regs[]；
 * 2. 再逐个 regmap_read() 读取各寄存器；
 * 3. 输出成文本快照。
 *
 * 为什么故意不用“直接打印 regs[]”？
 * 因为教学上想强调：
 *   用户看到的是 regmap API 暴露出来的寄存器视图，
 *   而不是绕过 regmap 直接摸内部数组。
 */
static int demo_regmap_snapshot_show(struct seq_file *m, void *v)
{
    struct demo_regmap_priv *priv = m->private;
    unsigned int val;
    unsigned int i;
    int ret;

    demo_regmap_refresh_view(priv);

    seq_printf(m, "module=%s\n", DRV_NAME);
    seq_printf(m, "label=%s\n", priv->label ? priv->label : "(null)");
    seq_printf(m, "match_name=%s\n", priv->match_name ? priv->match_name : "(null)");
    seq_printf(m, "linux_irq=%d\n", priv->linux_irq);
    seq_printf(m, "raw_reg=<%#x %#x %#x %#x>\n",
               priv->raw_reg[0], priv->raw_reg[1],
               priv->raw_reg[2], priv->raw_reg[3]);
    seq_printf(m, "raw_irq=<%#x %#x %#x>\n",
               priv->raw_irq[0], priv->raw_irq[1], priv->raw_irq[2]);
    seq_puts(m, "----------------------------------------\n");

    for (i = 0; i < ARRAY_SIZE(demo_regmap_reg_desc); i++) {
        ret = regmap_read(priv->regmap, demo_regmap_reg_desc[i].reg, &val);
        if (ret) {
            seq_printf(m, "%-16s read-error=%d\n",
                       demo_regmap_reg_desc[i].name, ret);
            continue;
        }

        seq_printf(m, "%-16s reg=%#04x val=%#010x (%u)\n",
                   demo_regmap_reg_desc[i].name,
                   demo_regmap_reg_desc[i].reg,
                   val, val);
    }

    return 0;
}

/*
 * demo_regmap_snapshot_open()
 * ---------------------------
 * standard seq_file open 写法：
 * 用 single_open() 把 inode->i_private 里的 priv 透传给 show 回调。
 *
 * 这是 debugfs/procfs 文本只读接口里最常见的写法之一，值得记住。
 */
static int demo_regmap_snapshot_open(struct inode *inode, struct file *file)
{
    return single_open(file, demo_regmap_snapshot_show, inode->i_private);
}

/*
 * snapshot file_operations
 *
 * 用户态使用方式：
 *   cat /sys/kernel/debug/demo_regmap/snapshot
 */
static const struct file_operations demo_regmap_snapshot_fops = {
    .owner = THIS_MODULE,
    .open = demo_regmap_snapshot_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/*
 * demo_regmap_poke_write()
 * ------------------------
 * poke 的写回调，用来验证 regmap_write() 路径。
 *
 * 输入格式：
 *   echo "0x28 50" > /sys/kernel/debug/demo_regmap/poke
 *
 * 含义：
 * - 把寄存器 0x28(DEMO_REG_WORK_MS) 写成 50
 * - 之后 worker 的模拟重活时长会变成 50ms
 * - 再跑 function_graph 时，你会更容易在 trace 里看出 worker 比 top-half 长很多
 *
 * 这里用到的新 helper：
 * - copy_from_user(): 把用户缓冲区复制到内核栈缓冲区
 * - strim()         : 去掉首尾空白和换行
 * - strsep()        : 以空格/制表/换行切分 token
 * - kstrtouint()    : 把字符串安全转换成 unsigned int
 */
static ssize_t demo_regmap_poke_write(struct file *file,
                                      const char __user *buf,
                                      size_t count,
                                      loff_t *ppos)
{
    struct demo_regmap_priv *priv = file->private_data;
    char kbuf[64];
    char *cur;
    char *tok_reg;
    char *tok_val;
    unsigned int reg;
    unsigned int val;
    int ret;
    size_t len;

    if (!count)
        return 0;

    len = min(count, sizeof(kbuf) - 1);
    if (copy_from_user(kbuf, buf, len))
        return -EFAULT;
    kbuf[len] = '\0';

    cur = strim(kbuf);
    tok_reg = strsep(&cur, " \t\n");
    tok_val = strsep(&cur, " \t\n");
    if (!tok_reg || !tok_val || !*tok_reg || !*tok_val)
        return -EINVAL;

    ret = kstrtouint(tok_reg, 0, &reg);
    if (ret)
        return ret;
    ret = kstrtouint(tok_val, 0, &val);
    if (ret)
        return ret;

    ret = regmap_write(priv->regmap, reg, val);
    if (ret)
        return ret;

    dev_info(priv->dev, "poke reg=%#x val=%#x ok\n", reg, val);
    return count;
}

static const struct file_operations demo_regmap_poke_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .write = demo_regmap_poke_write,
    .llseek = noop_llseek,
};

/*
 * demo_regmap_trigger_write()
 * ---------------------------
 * trigger 的写回调，用来“软件触发”一次 fake IRQ。
 *
 * 输入格式：
 *   echo 1 > /sys/kernel/debug/demo_regmap/trigger
 *   echo 5 > /sys/kernel/debug/demo_regmap/trigger
 *
 * 作用：
 * - 用户在 shell 中写 trigger；
 * - 驱动从 debugfs write 回调进入；
 * - 在循环里调用 generic_handle_irq(priv->linux_irq)；
 * - IRQ core 按标准派发链调用 demo_regmap_handler()；
 * - handler 只负责 queue_work()；
 * - 真正的后处理在 worker 执行。
 *
 * 为什么 Day13 要在 generic_handle_irq() 前后包一层 local_irq_save/restore？
 * -------------------------------------------------------------------------
 * day12 中你已经遇到过 warning：
 *   "irq <n> handler demo_regmap_handler enabled interrupts"
 *
 * 根因不是 handler 主动开中断，而是：
 * - generic_handle_irq() 更像“硬中断派发入口”；
 * - 但 debugfs write 本身运行在普通进程上下文，本地中断通常是开的；
 * - 这样 fake IRQ 的调用现场就不像真实 hardirq 进入时的约束环境。
 *
 * Day13 为了让 function_graph 抓到更干净的 IRQ 派发路径，
 * 在每次 software-trigger 前后配合 local_irq_save()/local_irq_restore()，
 * 让执行现场更接近真实硬中断入口。
 *
 * 这段代码也是 day13 trace 的“总入口”：
 * function_graph 最推荐先从这里开始看。
 */
static ssize_t demo_regmap_trigger_write(struct file *file,
                                         const char __user *buf,
                                         size_t count,
                                         loff_t *ppos)
{
    struct demo_regmap_priv *priv = file->private_data;
    char kbuf[32];
    unsigned int times;
    unsigned int ctrl;
    size_t len;
    unsigned int i;
    int ret;

    if (!count)
        return 0;

    len = min(count, sizeof(kbuf) - 1);
    if (copy_from_user(kbuf, buf, len))
        return -EFAULT;
    kbuf[len] = '\0';

    ret = kstrtouint(strim(kbuf), 0, &times);
    if (ret)
        return ret;

    if (!times)
        times = 1;                      /* 允许 echo 0 退化成触发 1 次，简化用户操作 */
    if (times > 100000)
        return -EINVAL;                 /* 给软件触发加个上限，避免误写造成过大负载 */

    ret = regmap_read(priv->regmap, DEMO_REG_CTRL, &ctrl);
    if (ret)
        return ret;
    if (!(ctrl & DEMO_CTRL_ENABLE))
        return -EPERM;                  /* 如果 CTRL.enable 被清掉，则不允许继续触发 */

    for (i = 0; i < times; i++) {
        unsigned long flags;

        local_irq_save(flags);          /* 模拟更接近 hardirq 的进入现场 */
        generic_handle_irq(priv->linux_irq);
        local_irq_restore(flags);       /* 恢复原现场，避免影响后续普通执行流 */
    }

    return count;
}

static const struct file_operations demo_regmap_trigger_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .write = demo_regmap_trigger_write,
    .llseek = noop_llseek,
};

/*
 * demo_regmap_create_debugfs()
 * ----------------------------
 * 创建本驱动的 debugfs 目录和三个实验入口：
 * - snapshot : 读寄存器快照
 * - poke     : 写配置寄存器
 * - trigger  : 触发 fake IRQ
 *
 * 为什么这三个入口很适合教学：
 * - snapshot：看状态
 * - poke    ：改配置
 * - trigger ：跑路径
 *
 * 这三者加在一起，就形成了“可观测 + 可控制 + 可重复”的最小实验闭环。
 */
static int demo_regmap_create_debugfs(struct demo_regmap_priv *priv)
{
    priv->debugfs_dir = debugfs_create_dir(DEMO_DEBUGFS_DIR, NULL);
    if (IS_ERR_OR_NULL(priv->debugfs_dir)) {
        dev_warn(priv->dev, "debugfs unavailable\n");
        priv->debugfs_dir = NULL;
        return 0;
    }

    priv->debugfs_snapshot = debugfs_create_file("snapshot", 0444,
                                                 priv->debugfs_dir, priv,
                                                 &demo_regmap_snapshot_fops);
    priv->debugfs_poke = debugfs_create_file("poke", 0200,
                                             priv->debugfs_dir, priv,
                                             &demo_regmap_poke_fops);
    priv->debugfs_trigger = debugfs_create_file("trigger", 0200,
                                                priv->debugfs_dir, priv,
                                                &demo_regmap_trigger_fops);

    if (IS_ERR_OR_NULL(priv->debugfs_snapshot) ||
        IS_ERR_OR_NULL(priv->debugfs_poke) ||
        IS_ERR_OR_NULL(priv->debugfs_trigger)) {
        dev_err(priv->dev, "failed to create debugfs files\n");
        debugfs_remove_recursive(priv->debugfs_dir);
        priv->debugfs_dir = NULL;
        return -ENOMEM;
    }

    return 0;
}

/*
 * demo_regmap_destroy_debugfs()
 * -----------------------------
 * 和 create_debugfs 成对出现，统一递归删除目录树。
 *
 * debugfs_remove_recursive() 是 debugfs 清理时最方便也最常见的 API：
 * 给它目录节点，它会把下面的子文件一起删掉。
 */
static void demo_regmap_destroy_debugfs(struct demo_regmap_priv *priv)
{
    debugfs_remove_recursive(priv->debugfs_dir);
    priv->debugfs_dir = NULL;
}

/*
 * demo_regmap_probe()
 * -------------------
 * probe 是 platform_driver 真正“接管设备”的入口。
 *
 * 你可以把它分成 7 个阶段来理解：
 * 1. 分配 priv，并挂到 platform_device 上
 * 2. 解析 DT 原始属性(raw reg/raw irq/label)
 * 3. 获取 MEM 资源和 Linux IRQ 号
 * 4. 初始化 workqueue / work
 * 5. 初始化 regmap，并写入 CTRL/WORK_MS 初值
 * 6. request_irq() 注册 top-half
 * 7. 创建 debugfs 入口
 *
 * 这个函数是 day09/day10/day11/day12/day13 多天知识汇聚的核心。
 */
static int demo_regmap_probe(struct platform_device *pdev)
{
    struct demo_regmap_priv *priv;
    struct resource *res;
    const struct of_device_id *match;
    struct device_node *np = pdev->dev.of_node;
    int ret;

    dev_info(&pdev->dev, "probe begin\n");

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->dev = &pdev->dev;
    priv->work_delay_ms = default_work_ms;
    mutex_init(&priv->reg_lock);
    platform_set_drvdata(pdev, priv);

    if (np) {
        of_property_read_string(np, "demo,label", &priv->label);
        demo_regmap_dump_raw_reg(&pdev->dev, np, priv);
        demo_regmap_dump_raw_irq(&pdev->dev, np, priv);
    }

    match = of_match_device(pdev->dev.driver->of_match_table, &pdev->dev);
    if (match)
        priv->match_name = match->compatible;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "no MEM resource found\n");
        return -ENODEV;
    }
    memcpy(&priv->mem, res, sizeof(*res));

    priv->linux_irq = platform_get_irq(pdev, 0);
    if (priv->linux_irq < 0) {
        dev_err(&pdev->dev, "platform_get_irq failed: %d\n", priv->linux_irq);
        return priv->linux_irq;
    }

    INIT_WORK(&priv->work, demo_regmap_workfn);
    priv->wq = alloc_ordered_workqueue("%s_wq", WQ_MEM_RECLAIM, DRV_NAME);
    if (!priv->wq)
        return -ENOMEM;

    priv->regmap = devm_regmap_init(&pdev->dev, NULL, priv, &demo_regmap_config);
    if (IS_ERR(priv->regmap)) {
        ret = PTR_ERR(priv->regmap);
        dev_err(&pdev->dev, "devm_regmap_init failed: %d\n", ret);
        goto err_destroy_wq;
    }

    /*
     * 这里刻意通过 regmap_write() 初始化可写寄存器，而不是直接改 regs[]：
     *
     * 1. 让模块加载阶段就先走通一次 regmap 写路径；
     * 2. 让“内部初始化”和“用户 poke 写寄存器”共用同一套入口。
     */
    ret = regmap_write(priv->regmap, DEMO_REG_CTRL, DEMO_CTRL_ENABLE);
    if (ret) {
        dev_err(&pdev->dev, "regmap_write CTRL failed: %d\n", ret);
        goto err_destroy_wq;
    }

    ret = regmap_write(priv->regmap, DEMO_REG_WORK_MS, priv->work_delay_ms);
    if (ret) {
        dev_err(&pdev->dev, "regmap_write WORK_MS failed: %d\n", ret);
        goto err_destroy_wq;
    }

    demo_regmap_refresh_view(priv);

    ret = request_irq(priv->linux_irq, demo_regmap_handler,
                      IRQF_SHARED, DRV_NAME, priv);
    if (ret) {
        dev_err(&pdev->dev, "request_irq(%d) failed: %d\n", priv->linux_irq, ret);
        goto err_destroy_wq;
    }

    ret = demo_regmap_create_debugfs(priv);
    if (ret)
        goto err_free_irq;

    dev_info(&pdev->dev,
             "probe ok: label=%s match=%s mem=[%pa-%pa] linux_irq=%d work_ms=%u\n",
             priv->label ? priv->label : "(null)",
             priv->match_name ? priv->match_name : "(null)",
             &priv->mem.start, &priv->mem.end,
             priv->linux_irq, priv->work_delay_ms);
    dev_info(&pdev->dev,
             "debugfs: /sys/kernel/debug/%s/{snapshot,poke,trigger}\n",
             DEMO_DEBUGFS_DIR);

    return 0;

err_free_irq:
    free_irq(priv->linux_irq, priv);
err_destroy_wq:
    destroy_workqueue(priv->wq);
    priv->wq = NULL;
    return ret;
}

/*
 * demo_regmap_cleanup()
 * ---------------------
 * 把 remove 阶段需要做的动作集中到一个 helper 里，兼容不同内核版本 remove 原型差异。
 *
 * 清理顺序也值得注意：
 * 1. 先删 debugfs，避免用户态继续访问；
 * 2. flush/destroy workqueue，确保没有未完成 bottom-half；
 * 3. free_irq，解除中断处理函数绑定。
 */
static void demo_regmap_cleanup(struct platform_device *pdev)
{
    struct demo_regmap_priv *priv = platform_get_drvdata(pdev);

    if (!priv)
        return;

    demo_regmap_destroy_debugfs(priv);

    if (priv->wq) {
        flush_workqueue(priv->wq);      /* 等待已排队 work 跑完，避免卸载时残留执行 */
        destroy_workqueue(priv->wq);
        priv->wq = NULL;
    }

    free_irq(priv->linux_irq, priv);
    dev_info(&pdev->dev, "remove done\n");
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void demo_regmap_remove(struct platform_device *pdev)
{
    demo_regmap_cleanup(pdev);
}
#else
static int demo_regmap_remove(struct platform_device *pdev)
{
    demo_regmap_cleanup(pdev);
    return 0;
}
#endif

/*
 * of_match_table
 * --------------
 * compatible 命中后，内核会把这个设备交给本驱动的 platform_driver 处理。
 *
 * 设备树里的节点写的是：
 *   compatible = "demo,regmap-pdrv";
 *
 * 一旦命中，probe() 就会被调用。
 */
static const struct of_device_id demo_regmap_match[] = {
    { .compatible = "demo,regmap-pdrv" },
    { }
};
MODULE_DEVICE_TABLE(of, demo_regmap_match);

/*
 * platform_driver
 * ---------------
 * 这是 platform 总线驱动的“注册对象”，告诉内核：
 * - 我能匹配哪些 DT 设备
 * - 设备匹配成功后进哪个 probe
 * - 卸载时走哪个 remove
 *
 * 结合 day09 以来的知识，建议把它记成一句话：
 * “of_match_table 决定谁来找我，probe/remove 决定我如何接管和释放设备”。
 */
static struct platform_driver demo_regmap_driver = {
    .probe = demo_regmap_probe,          /* 匹配成功后的入口：解析资源、初始化 regmap/workqueue、注册 IRQ、创建 debugfs */
    .remove = demo_regmap_remove,        /* 解绑/卸载时的出口：删 debugfs、flush workqueue、free_irq */
    .driver = {
        .name = DRV_NAME,                /* 驱动名字：出现在内核日志和驱动模型中 */
        .of_match_table = demo_regmap_match, /* DT 匹配表：compatible 命中后才会进入 probe */
    },
};

/*
 * module init/exit
 * ----------------
 * 模块入口只负责把 platform_driver 注册到内核；
 * 真正对具体设备做初始化，是在匹配成功后由 probe() 完成的。
 */
static int __init demo_regmap_init(void)
{
    pr_info(DRV_NAME ": module init\n");
    return platform_driver_register(&demo_regmap_driver);
}

static void __exit demo_regmap_exit(void)
{
    pr_info(DRV_NAME ": module exit\n");
    platform_driver_unregister(&demo_regmap_driver);
}

module_init(demo_regmap_init);
module_exit(demo_regmap_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richer Wong");
MODULE_DESCRIPTION("Day13 demo: function_graph tracing over regmap-backed IRQ/workqueue path");
