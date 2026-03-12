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
#define DEMO_VERSION_CODE    0x00001200

/*
 * Day12 继续保留 day11 的“模拟重活”思路。
 * worker 中默认 sleep 20ms，方便你观察：
 * 1. top-half 很快返回
 * 2. worker 真正在 process context 中承担重活
 * 3. 寄存器快照里的 latency/work 统计会随之变化
 */
static unsigned int default_work_ms = 20;
module_param(default_work_ms, uint, 0644);
MODULE_PARM_DESC(default_work_ms, "default simulated heavy work duration in milliseconds");

/* --------------------------
 * Day12 的教学寄存器地图
 * --------------------------
 * 这次不直接访问真实 MMIO，而是用 shadow regs[] + regmap 封装出一组“寄存器视图”。
 * 这样可以把 day11 的运行时状态整理成更接近真实驱动的组织方式。
 */
#define DEMO_REG_CTRL             0x00 /* 控制寄存器：bit0=enable */
#define DEMO_REG_STATUS           0x04 /* 状态寄存器：enable/pending/busy */
#define DEMO_REG_IRQ_COUNT        0x08 /* top-half 总触发次数 */
#define DEMO_REG_WORK_RUNS        0x0c /* worker 实际运行次数 */
#define DEMO_REG_WORK_ITEMS       0x10 /* worker 实际处理事件总数 */
#define DEMO_REG_PENDING_EVENTS   0x14 /* 当前待处理事件数 */
#define DEMO_REG_LAST_BATCH       0x18 /* 最近一轮 worker 处理的 batch */
#define DEMO_REG_LAST_LATENCY_US  0x1c /* 最近一次粗略延迟（微秒） */
#define DEMO_REG_MAX_LATENCY_US   0x20 /* 历史最大粗略延迟（微秒） */
#define DEMO_REG_AVG_LATENCY_US   0x24 /* 平均粗略延迟（微秒） */
#define DEMO_REG_WORK_MS          0x28 /* worker 模拟重活时长（毫秒） */
#define DEMO_REG_VERSION          0x2c /* 教学版本号 */

#define DEMO_REG_STRIDE           4
#define DEMO_MAX_REGISTER         DEMO_REG_VERSION
#define DEMO_REG_COUNT            ((DEMO_MAX_REGISTER / DEMO_REG_STRIDE) + 1)

/* CTRL 的 bit 定义 */
#define DEMO_CTRL_ENABLE          BIT(0)
#define DEMO_CTRL_WRITABLE_MASK   DEMO_CTRL_ENABLE

/* STATUS 的 bit 定义 */
#define DEMO_STATUS_PENDING       BIT(0)
#define DEMO_STATUS_WORK_BUSY     BIT(1)
#define DEMO_STATUS_ENABLED       BIT(2)

/*
 * day12 的私有结构体保留了 day11 的骨架：
 * - platform/DT 资源解析
 * - top-half / workqueue / latency 统计
 * - 新增 regmap + debugfs
 */
struct demo_regmap_priv {
    struct device *dev;                  /* 设备对象，probe 时由 platform_device 派生得到 */
    struct resource mem;                 /* DT 解析得到的 MMIO 资源（day12 仍保留资源解析教学链路） */
    int linux_irq;                       /* Linux IRQ 号（由 DT interrupts 解析得到） */

    u32 raw_reg[4];                      /* 原始 reg cells，便于教学观察 DT 解析结果 */
    u32 raw_irq[3];                      /* 原始 interrupts cells，便于教学观察 DT 解析结果 */
    const char *label;                   /* 设备标签，通常来自 DT 属性 demo,label */
    const char *match_name;              /* 匹配到的 compatible 名称 */

    atomic64_t irq_count;                /* top-half 被触发的总次数 */
    atomic64_t work_runs;                /* worker 实际运行次数 */
    atomic64_t work_items;               /* worker 实际处理事件总数 */

    atomic_t pending_events;             /* 当前累计待处理事件数：top-half 加，worker 取走 */
    atomic_t last_batch;                 /* 最近一次 worker 批处理的 batch 大小 */
    atomic_t work_busy;                  /* worker 是否正在处理当前一轮任务 */

    atomic64_t first_pending_irq_ns;     /* 当前 batch 第一个 pending IRQ 的时间戳（ns） */
    atomic64_t last_irq_ns;              /* 最近一次 IRQ 时间戳（ns） */

    atomic64_t last_latency_ns;          /* 最近一次粗略延迟（ns） */
    atomic64_t max_latency_ns;           /* 历史最大粗略延迟（ns） */
    atomic64_t sum_latency_ns;           /* 延迟累计值（ns），用于计算平均值 */
    atomic64_t latency_samples;          /* 延迟样本数 */

    struct workqueue_struct *wq;         /* 驱动私有 workqueue，用于承载 bottom-half */
    struct work_struct work;             /* 具体 work 对象，由 top-half queue 进去 */
    unsigned int work_delay_ms;          /* 当前 worker 模拟重活时长（毫秒） */

    struct regmap *regmap;               /* regmap 句柄：统一寄存器读写入口 */
    struct mutex reg_lock;               /* 保护 shadow regs[] 的锁 */
    u32 regs[DEMO_REG_COUNT];            /* 教学用 shadow registers */

    struct dentry *debugfs_dir;          /* /sys/kernel/debug/demo_regmap 目录 */
    struct dentry *debugfs_snapshot;     /* snapshot 节点：只读寄存器快照 */
    struct dentry *debugfs_poke;         /* poke 节点：写寄存器验证 regmap write 路径 */
    struct dentry *debugfs_trigger;      /* trigger 节点：软件触发 fake IRQ */
};

static void demo_regmap_dump_raw_reg(struct device *dev,
                                     struct device_node *np,
                                     struct demo_regmap_priv *priv)
{
    int ret;

    ret = of_property_read_u32_array(np, "reg", priv->raw_reg, ARRAY_SIZE(priv->raw_reg));
    if (ret) {
        dev_warn(dev, "raw DT reg read failed: %d\n", ret);
        return;
    }

    dev_info(dev, "raw DT reg cells: <%#x %#x %#x %#x>\n",
             priv->raw_reg[0], priv->raw_reg[1],
             priv->raw_reg[2], priv->raw_reg[3]);
}

static void demo_regmap_dump_raw_irq(struct device *dev,
                                     struct device_node *np,
                                     struct demo_regmap_priv *priv)
{
    int ret;

    ret = of_property_read_u32_array(np, "interrupts", priv->raw_irq, ARRAY_SIZE(priv->raw_irq));
    if (ret) {
        dev_warn(dev, "raw DT interrupts read failed: %d\n", ret);
        return;
    }

    dev_info(dev, "raw DT interrupts cells: <%#x %#x %#x>\n",
             priv->raw_irq[0], priv->raw_irq[1], priv->raw_irq[2]);
}

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
 * 把 day11 的运行态统计刷新到 day12 的寄存器视图里。
 * 这一步非常重要：
 * top-half / worker 仍然主要更新原子变量，
 * 而 debugfs snapshot 读取时，会先把这些变量同步成 shadow regs[]，
 * 再通过 regmap_read() 逐个读出来。
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
    priv->regs[DEMO_REG_IRQ_COUNT / DEMO_REG_STRIDE] = (u32)atomic64_read(&priv->irq_count);
    priv->regs[DEMO_REG_WORK_RUNS / DEMO_REG_STRIDE] = (u32)atomic64_read(&priv->work_runs);
    priv->regs[DEMO_REG_WORK_ITEMS / DEMO_REG_STRIDE] = (u32)atomic64_read(&priv->work_items);
    priv->regs[DEMO_REG_PENDING_EVENTS / DEMO_REG_STRIDE] = (u32)atomic_read(&priv->pending_events);
    priv->regs[DEMO_REG_LAST_BATCH / DEMO_REG_STRIDE] = (u32)atomic_read(&priv->last_batch);
    priv->regs[DEMO_REG_LAST_LATENCY_US / DEMO_REG_STRIDE] = (u32)(atomic64_read(&priv->last_latency_ns) / 1000ULL);
    priv->regs[DEMO_REG_MAX_LATENCY_US / DEMO_REG_STRIDE] = (u32)(atomic64_read(&priv->max_latency_ns) / 1000ULL);

    samples = atomic64_read(&priv->latency_samples);
    sum_ns = atomic64_read(&priv->sum_latency_ns);
    if (samples)
        avg_ns = div64_u64(sum_ns, samples);

    priv->regs[DEMO_REG_AVG_LATENCY_US / DEMO_REG_STRIDE] = (u32)(avg_ns / 1000ULL);
    priv->regs[DEMO_REG_WORK_MS / DEMO_REG_STRIDE] = priv->work_delay_ms;
    priv->regs[DEMO_REG_VERSION / DEMO_REG_STRIDE] = DEMO_VERSION_CODE;

    mutex_unlock(&priv->reg_lock);
}

static bool demo_regmap_is_valid_reg(unsigned int reg)
{
    if (reg > DEMO_MAX_REGISTER)
        return false;
    if (reg % DEMO_REG_STRIDE)
        return false;
    return true;
}

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
 * regmap 的“读后端”：
 * 这里不访问真实硬件，而是从 shadow regs[] 中取值。
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
 * regmap 的“写后端”：
 * 这里只允许写 CTRL 和 WORK_MS，其他寄存器都视为只读。
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
            val = 5000;
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
 * bottom-half：worker 继续沿用 day11 的职责
 * 1. 批量取走 pending 事件
 * 2. 计算粗略延迟
 * 3. 模拟重活（可以 sleep）
 *
 * Day12 的新变化不在“谁来干活”，而在“统计结果如何被组织成寄存器视图”。
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
            msleep(priv->work_delay_ms);

        atomic_set(&priv->work_busy, 0);
    }
}

/*
 * top-half：仍然保持最小动作。
 * 这能保证 day11 的教学目标继续成立：
 * “中断上下文只抢现场，不在这里做重活”。
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
 * snapshot：先刷新寄存器视图，再通过 regmap_read() 逐个读取。
 * 这条路径就是 day12 最直接的“regmap 读路径”验证。
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

static int demo_regmap_snapshot_open(struct inode *inode, struct file *file)
{
    return single_open(file, demo_regmap_snapshot_show, inode->i_private);
}

static const struct file_operations demo_regmap_snapshot_fops = {
    .owner = THIS_MODULE,
    .open = demo_regmap_snapshot_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/*
 * poke：写寄存器验证 regmap_write() 路径。
 * 输入格式：
 *   echo "0x28 50" > /sys/kernel/debug/demo_regmap/poke
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
 * trigger：软件方式触发 fake IRQ。
 * 这里本质上是为了延续 day10/day11 的教学环境，让你不用真实硬件中断源，
 * 也能把 top-half -> workqueue -> regmap snapshot 这条链路全部观察一遍。
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
        times = 1;
    if (times > 100000)
        return -EINVAL;

    ret = regmap_read(priv->regmap, DEMO_REG_CTRL, &ctrl);
    if (ret)
        return ret;
    if (!(ctrl & DEMO_CTRL_ENABLE))
        return -EPERM;

    for (i = 0; i < times; i++)
        generic_handle_irq(priv->linux_irq);

    return count;
}

static const struct file_operations demo_regmap_trigger_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .write = demo_regmap_trigger_write,
    .llseek = noop_llseek,
};

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

static void demo_regmap_destroy_debugfs(struct demo_regmap_priv *priv)
{
    debugfs_remove_recursive(priv->debugfs_dir);
    priv->debugfs_dir = NULL;
}

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
     * 通过 regmap_write() 初始化可写控制寄存器：
     * 这样在模块加载阶段就把 regmap 写路径先跑通一次。
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

static void demo_regmap_cleanup(struct platform_device *pdev)
{
    struct demo_regmap_priv *priv = platform_get_drvdata(pdev);

    if (!priv)
        return;

    demo_regmap_destroy_debugfs(priv);

    if (priv->wq) {
        flush_workqueue(priv->wq);
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

static const struct of_device_id demo_regmap_match[] = {
    { .compatible = "demo,regmap-pdrv" }, /* DT compatible 命中后，内核会调用本驱动的 probe */
    { }
};
MODULE_DEVICE_TABLE(of, demo_regmap_match);

static struct platform_driver demo_regmap_driver = {
    .probe = demo_regmap_probe,          /* 设备匹配成功后的入口：解析 DT、初始化 regmap、注册 IRQ、创建 debugfs */
    .remove = demo_regmap_remove,        /* 设备解绑/模块卸载时的出口：销毁 debugfs、flush/destroy workqueue、free_irq */
    .driver = {
        .name = DRV_NAME,                /* 驱动名字，供内核驱动模型和日志使用 */
        .of_match_table = demo_regmap_match, /* DT 匹配表：compatible 命中后触发 probe */
    },
};

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
MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Day12 demo: regmap-backed shadow registers with debugfs snapshot");
