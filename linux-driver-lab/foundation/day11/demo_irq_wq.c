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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/math64.h>

#define DRV_NAME          "demo_irq_wq"
#define PROC_STATS_NAME   "demo_irq_wq_stats"
#define PROC_TRIGGER_NAME "demo_irq_wq_trigger"
#define PROC_BUF_SZ       32

/*
 * day11 默认在 worker 里模拟 20ms 的“重活”。
 * 这个值不是为了追求真实性，而是为了教学：
 * 你能明显看到 top-half 很快返回，而真正耗时动作发生在 workqueue 中。
 */
static unsigned int work_ms = 20;
module_param(work_ms, uint, 0644);
MODULE_PARM_DESC(work_ms, "simulated heavy work duration in milliseconds");

/*
 * 这份私有数据把 day09/day10/day11 的关键状态串起来：
 * 1. DT / platform 资源：mem、linux_irq、raw_reg、raw_irq
 * 2. top-half 统计：irq_count
 * 3. bottom-half 统计：work_runs、work_items、pending_events
 * 4. 延迟统计：last/max/sum/samples
 * 5. workqueue 对象：wq、work
 */
struct demo_irq_wq_priv {
    struct device *dev;                  /* 设备对象，probe 时由 platform_device 派生得到 */
    struct resource mem;                 /* 设备的 MMIO 资源范围（从 DT 的 reg 解析而来） */
    int linux_irq;                       /* Linux IRQ 号（从 DT 的 interrupts 解析并映射后得到） */

    u32 raw_reg[4];                      /* 原始 reg 属性内容，便于教学和调试观察 DT 解析结果 */
    u32 raw_irq[3];                      /* 原始 interrupts 属性内容，便于教学和调试观察 DT 解析结果 */
    const char *label;                   /* 设备标签/名字，通常来自 DT 节点属性，便于打印日志 */
    const char *match_name;              /* 设备匹配名，通常对应 compatible 或驱动匹配信息 */

    atomic64_t irq_count;                /* top-half 被触发的总次数（每进一次中断处理函数就加 1） */
    atomic64_t work_runs;                /* workqueue 的 worker 实际运行次数 */
    atomic64_t work_items;               /* worker 实际处理的事件总数（可与 irq_count 对照） */

    atomic_t pending_events;             /* 当前累计待处理事件数：top-half 加，worker 取走/清空 */
    atomic_t last_batch;                 /* 最近一次 worker 批量处理的事件个数 */

    atomic64_t first_pending_irq_ns;     /* 当前这一批 pending 中，第一个 IRQ 到来时间戳（ns） */
    atomic64_t last_irq_ns;              /* 最近一次 IRQ 到来时间戳（ns） */

    atomic64_t last_latency_ns;          /* 最近一次统计到的延迟（ns） */
    atomic64_t max_latency_ns;           /* 历史最大延迟（ns） */
    atomic64_t sum_latency_ns;           /* 延迟累计值（ns），用于计算平均延迟 */
    atomic64_t latency_samples;          /* 延迟样本数，用于计算平均延迟 */

    struct workqueue_struct *wq;         /* 驱动私有 workqueue，用于承载 bottom-half 重活 */
    struct work_struct work;             /* 具体的 work 对象，由 top-half queue 到 workqueue */

    struct proc_dir_entry *proc_stats;   /* /proc 统计节点，用于导出计数和延迟数据 */
    struct proc_dir_entry *proc_trigger; /* /proc 触发节点，用于软件方式手动触发 IRQ 测试 */
};

/*
 * proc 读写接口拿不到 pdev，这里继续沿用教学友好的单实例全局指针。
 */
static struct demo_irq_wq_priv *g_demo_priv;

static void demo_irq_wq_dump_raw_reg(struct device *dev,
                                     struct device_node *np,
                                     struct demo_irq_wq_priv *priv)
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

static void demo_irq_wq_dump_raw_irq(struct device *dev,
                                     struct device_node *np,
                                     struct demo_irq_wq_priv *priv)
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
 * 更新延迟统计
 * 这里记录的是 粗略调度延迟：
 * 从 top-half 第一次观察到当前批次 pending 事件，到 worker 真正开始执行之间的时间差
 *
 * 它不是硬实时 benchmark，只是为了把 day11 的核心教学目标可视化：
 * 重活已经不在中断上下文里做，而是排队后由 worker 接手。
 */
static void demo_irq_wq_update_latency(struct demo_irq_wq_priv *priv, u64 latency_ns)
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
 * bottom-half：workqueue worker
 * 1. 读取 batch 并统计粗略延迟
 * 2. 模拟耗时处理（可 sleep）
 * 3. 记录本轮处理了多少事件
 *
 * 注意：这里故意使用 msleep()，为了强调它已经处于进程上下文，和 top-half 的硬中断上下文不同。
 */
static void demo_irq_wq_workfn(struct work_struct *work)
{
    struct demo_irq_wq_priv *priv;

    priv = container_of(work, struct demo_irq_wq_priv, work);

    for (;;) {
        int batch;
        u64 work_start_ns;
        u64 first_ns;
        u64 latency_ns;

        batch = atomic_xchg(&priv->pending_events, 0);
        if (batch <= 0)
            break;

        atomic_set(&priv->last_batch, batch);
        atomic64_inc(&priv->work_runs);
        atomic64_add(batch, &priv->work_items);

        work_start_ns = ktime_get_ns();
        first_ns = atomic64_read(&priv->first_pending_irq_ns);
        if (work_start_ns >= first_ns)
            latency_ns = work_start_ns - first_ns;
        else
            latency_ns = 0;

        demo_irq_wq_update_latency(priv, latency_ns);

        dev_info_ratelimited(priv->dev,
                             "worker start batch=%d latency=%llu us pending_now=%d\n",
                             batch,
                             (unsigned long long)(latency_ns / 1000),
                             atomic_read(&priv->pending_events));
        /*
         * 这里故意放一个可 sleep 的重活模拟
         * 如果把这类动作塞回中断处理函数，就违背了 day11 的目标
         */
        if (work_ms)
            msleep(work_ms);
    }
}

/*
 * 最小 top-half，只做四件事：
 * 1. 记当前时间
 * 2. 增加 irq_count
 * 3. 把 pending 事件数 +1
 * 4. queue_work() 把真正处理下沉到 bottom-half
 * 整个思路就是：中断上下文只抢现场，不做重活
 */
static irqreturn_t demo_irq_wq_handler(int irq, void *dev_id)
{
    struct demo_irq_wq_priv *priv = dev_id;
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

static int demo_irq_wq_stats_show(struct seq_file *m, void *v)
{
    struct demo_irq_wq_priv *priv = g_demo_priv;
    u64 last_ns;
    u64 max_ns;
    u64 sum_ns;
    u64 samples;
    u64 avg_ns = 0;

    if (!priv) {
        seq_puts(m, "driver not ready\n");
        return 0;
    }

    last_ns = atomic64_read(&priv->last_latency_ns);
    max_ns = atomic64_read(&priv->max_latency_ns);
    sum_ns = atomic64_read(&priv->sum_latency_ns);
    samples = atomic64_read(&priv->latency_samples);
    if (samples)
        avg_ns = div64_u64(sum_ns, samples);

    seq_printf(m, "module=%s\n", DRV_NAME);
    seq_printf(m, "label=%s\n", priv->label ? priv->label : "none");
    seq_printf(m, "linux_irq=%d\n", priv->linux_irq);
    seq_printf(m, "irq_count=%lld\n", (long long)atomic64_read(&priv->irq_count));
    seq_printf(m, "work_runs=%lld\n", (long long)atomic64_read(&priv->work_runs));
    seq_printf(m, "work_items=%lld\n", (long long)atomic64_read(&priv->work_items));
    seq_printf(m, "pending_events=%d\n", atomic_read(&priv->pending_events));
    seq_printf(m, "last_batch=%d\n", atomic_read(&priv->last_batch));
    seq_printf(m, "work_ms=%u\n", work_ms);
    seq_printf(m, "last_irq_ns=%lld\n", (long long)atomic64_read(&priv->last_irq_ns));
    seq_printf(m, "first_pending_irq_ns=%lld\n", (long long)atomic64_read(&priv->first_pending_irq_ns));
    seq_printf(m, "last_latency_ns=%llu\n", (unsigned long long)last_ns);
    seq_printf(m, "last_latency_us=%llu\n", (unsigned long long)(last_ns / 1000));
    seq_printf(m, "max_latency_us=%llu\n", (unsigned long long)(max_ns / 1000));
    seq_printf(m, "avg_latency_us=%llu\n", (unsigned long long)(avg_ns / 1000));
    seq_printf(m, "latency_samples=%llu\n", (unsigned long long)samples);
    seq_printf(m, "mem_start=0x%llx\n", (unsigned long long)priv->mem.start);
    seq_printf(m, "mem_size=0x%llx\n", (unsigned long long)resource_size(&priv->mem));

    return 0;
}

static int demo_irq_wq_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, demo_irq_wq_stats_show, NULL);
}

/*
 * /proc/demo_irq_wq_trigger 写接口
 *
 * 这里仍然不是让真实硬件去拉中断线，而是在进程上下文里对已申请 virq
 * 做软件注入，从而让你在没有真实硬件事件源的情况下，仍然能把：
 *
 *   request_irq -> top-half -> queue_work -> worker -> /proc 统计
 */
static ssize_t demo_irq_wq_trigger_write(struct file *file,
                                         const char __user *buf,
                                         size_t count,
                                         loff_t *ppos)
{
    struct demo_irq_wq_priv *priv = g_demo_priv;
    char kbuf[PROC_BUF_SZ];
    unsigned int n;
    size_t len;
    unsigned int i;
    int ret;

    if (!priv)
        return -ENODEV;

    len = min(count, sizeof(kbuf) - 1);
    if (copy_from_user(kbuf, buf, len))
        return -EFAULT;

    kbuf[len] = '\0';

    ret = kstrtouint(kbuf, 0, &n);
    if (ret)
        return ret;

    if (n == 0 || n > 100000)
        return -EINVAL;

    for (i = 0; i < n; i++) {
        ret = generic_handle_irq(priv->linux_irq);
        if (ret) {
            dev_err(priv->dev,
                    "generic_handle_irq failed at iter=%u ret=%d\n",
                    i, ret);
            return ret;
        }
    }

    dev_info(priv->dev,
             "soft-trigger injected %u times irq_count=%lld pending=%d\n",
             n,
             (long long)atomic64_read(&priv->irq_count),
             atomic_read(&priv->pending_events));

    return count;
}

/*
 * /proc/demo_irq_wq_stats 的文件操作集
 *
 * 读取 stats 节点时，最终会由 demo_irq_wq_stats_show()
 * 输出 top-half / bottom-half / latency 的各项统计
 */
static const struct proc_ops demo_irq_wq_stats_fops = {
    .proc_open    = demo_irq_wq_stats_open, /* 打开 stats 节点，建立 seq_file 读取上下文 */
    .proc_read    = seq_read,               /* 由 seq_file 框架逐步把内容拷给用户态 */
    .proc_lseek   = seq_lseek,              /* 支持常规偏移操作 */
    .proc_release = single_release,         /* 关闭文件时释放 single_open 相关资源 */
};

/*
 * /proc/demo_irq_wq_trigger 的文件操作集。
 *
 * 这里只实现写接口，因为它的职责就是接收“触发 N 次”的命令，
 * 然后通过 generic_handle_irq() 走完整的 top-half -> workqueue 链路。
 */
static const struct proc_ops demo_irq_wq_trigger_fops = {
    .proc_write = demo_irq_wq_trigger_write, /* 写入次数，执行软件注入 */
};

static void demo_irq_wq_remove_proc(struct demo_irq_wq_priv *priv)
{
    if (!priv)
        return;

    if (priv->proc_trigger) {
        proc_remove(priv->proc_trigger);
        priv->proc_trigger = NULL;
    }

    if (priv->proc_stats) {
        proc_remove(priv->proc_stats);
        priv->proc_stats = NULL;
    }
}

static int demo_irq_wq_create_proc(struct demo_irq_wq_priv *priv)
{
    priv->proc_stats = proc_create(PROC_STATS_NAME, 0444, NULL, &demo_irq_wq_stats_fops);
    if (!priv->proc_stats)
        return -ENOMEM;

    priv->proc_trigger = proc_create(PROC_TRIGGER_NAME, 0222, NULL, &demo_irq_wq_trigger_fops);
    if (!priv->proc_trigger) {
        demo_irq_wq_remove_proc(priv);
        return -ENOMEM;
    }

    return 0;
}

/*
 * Day11 的 DT 匹配表。
 * 当设备树里出现 compatible = "demo,irq-wq-pdrv" 的节点时，
 * platform 总线会把它与本驱动进行匹配；匹配成功后回调 probe。
 */
static const struct of_device_id demo_irq_wq_match[] = {
    {
        .compatible = "demo,irq-wq-pdrv",   /* 与 DT 节点 compatible 进行匹配 */
        .data = "irq-workqueue-of-match",   /* 给 probe 使用的匹配私有数据 */
    },
    { }                                     /* sentinel：匹配表结束 */
};
MODULE_DEVICE_TABLE(of, demo_irq_wq_match);

/*
 * probe 是 day11 的主战场
 *
 * 相比 day10，这里新增了：
 * - INIT_WORK()
 * - alloc_ordered_workqueue()
 * 这样 top-half 收到中断后，就能把真正的处理排给 worker
 */
static int demo_irq_wq_probe(struct platform_device *pdev)
{
    struct demo_irq_wq_priv *priv;
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;
    struct resource *mem;
    int irq;
    int ret;

    dev_info(dev, "probe start\n");

    if (!np) {
        dev_err(dev, "no device tree node attached\n");
        return -ENODEV;
    }

    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->dev = dev;
    priv->linux_irq = -1;
    priv->match_name = (const char *)device_get_match_data(dev);

    atomic64_set(&priv->irq_count, 0);
    atomic64_set(&priv->work_runs, 0);
    atomic64_set(&priv->work_items, 0);
    atomic_set(&priv->pending_events, 0);
    atomic_set(&priv->last_batch, 0);
    atomic64_set(&priv->first_pending_irq_ns, 0);
    atomic64_set(&priv->last_irq_ns, 0);
    atomic64_set(&priv->last_latency_ns, 0);
    atomic64_set(&priv->max_latency_ns, 0);
    atomic64_set(&priv->sum_latency_ns, 0);
    atomic64_set(&priv->latency_samples, 0);

    ret = of_property_read_string(np, "demo,label", &priv->label);
    if (ret)
        priv->label = "no-label";

    dev_info(dev, "of node full name: %s\n", np->full_name);
    dev_info(dev, "of match data: %s\n",
             priv->match_name ? priv->match_name : "none");
    dev_info(dev, "dt label: %s\n", priv->label);

    demo_irq_wq_dump_raw_reg(dev, np, priv);
    demo_irq_wq_dump_raw_irq(dev, np, priv);

    mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!mem) {
        dev_err(dev, "no MEM resource parsed from DT\n");
        return -ENODEV;
    }

    memcpy(&priv->mem, mem, sizeof(*mem));

    dev_info(dev, "parsed MEM resource: start=0x%llx end=0x%llx size=0x%llx\n",
             (unsigned long long)priv->mem.start,
             (unsigned long long)priv->mem.end,
             (unsigned long long)resource_size(&priv->mem));

    irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
        dev_err(dev, "platform_get_irq failed: %d\n", irq);
        return irq;
    }

    priv->linux_irq = irq;
    dev_info(dev, "parsed Linux IRQ: %d\n", priv->linux_irq);

    INIT_WORK(&priv->work, demo_irq_wq_workfn);

    /*
     * 用有序单线程工作队列，便于教学观察：
     * 同一模块的 work 按顺序处理，统计也更容易解释。
     */
    priv->wq = alloc_ordered_workqueue(DRV_NAME "_wq", 0);
    if (!priv->wq) {
        dev_err(dev, "alloc_ordered_workqueue failed\n");
        return -ENOMEM;
    }

    ret = request_irq(priv->linux_irq, demo_irq_wq_handler, 0, DRV_NAME, priv);
    if (ret) {
        dev_err(dev, "request_irq(%d) failed: %d\n", priv->linux_irq, ret);
        destroy_workqueue(priv->wq);
        priv->wq = NULL;
        return ret;
    }

    ret = demo_irq_wq_create_proc(priv);
    if (ret) {
        dev_err(dev, "create proc entries failed: %d\n", ret);
        free_irq(priv->linux_irq, priv);
        destroy_workqueue(priv->wq);
        priv->wq = NULL;
        return ret;
    }

    platform_set_drvdata(pdev, priv);
    g_demo_priv = priv;

    dev_info(dev, "request_irq + workqueue ready trigger via: echo 1 > /proc/%s\n", PROC_TRIGGER_NAME);
    dev_info(dev, "verify via: cat /proc/interrupts | grep %s\n", DRV_NAME);
    dev_info(dev, "stats via: cat /proc/%s\n", PROC_STATS_NAME);

    return 0;
}

static int demo_irq_wq_remove(struct platform_device *pdev)
{
    struct demo_irq_wq_priv *priv = platform_get_drvdata(pdev);

    g_demo_priv = NULL;
    demo_irq_wq_remove_proc(priv);

    if (priv->linux_irq >= 0)
        free_irq(priv->linux_irq, priv);

    if (priv->wq) {
        flush_workqueue(priv->wq);
        destroy_workqueue(priv->wq);
        priv->wq = NULL;
    }

    dev_info(&pdev->dev,
             "remove: linux_irq=%d irq_count=%lld work_runs=%lld work_items=%lld max_latency_us=%lld\n",
             priv->linux_irq,
             (long long)atomic64_read(&priv->irq_count),
             (long long)atomic64_read(&priv->work_runs),
             (long long)atomic64_read(&priv->work_items),
             (long long)(atomic64_read(&priv->max_latency_ns) / 1000));

    return 0;
}

static struct platform_driver demo_irq_wq_driver = {
    .probe = demo_irq_wq_probe,       /* 匹配成功后的入口：完成 request_irq、workqueue、/proc 等初始化 */
    .remove = demo_irq_wq_remove,     /* 设备解绑/模块卸载时的出口：停止 workqueue、释放 IRQ、清理 /proc */
    .driver = {
        .name = DRV_NAME,             /* 驱动名字，供内核驱动模型、sysfs 和日志使用 */
        .of_match_table = demo_irq_wq_match,
                                      /* OF 匹配表：compatible 命中的 DT 节点会交给本驱动 */
    },
};

static int __init demo_irq_wq_init(void)
{
    pr_info(DRV_NAME ": module init\n"); /* 模块加载入口：先向 platform 总线注册驱动 */
    return platform_driver_register(&demo_irq_wq_driver);
    /* 注册成功后，若系统中已存在匹配节点，接下来会继续进入 demo_irq_wq_probe() */
}

static void __exit demo_irq_wq_exit(void)
{
    pr_info(DRV_NAME ": module exit\n"); /* 模块卸载入口：准备把驱动从 platform 总线移除 */
    platform_driver_unregister(&demo_irq_wq_driver);
    /* 注销驱动；若设备仍绑定，会先触发 remove() */
}

module_init(demo_irq_wq_init);  
module_exit(demo_irq_wq_exit);  

MODULE_LICENSE("GPL");         
MODULE_AUTHOR("Richer Wong");
MODULE_DESCRIPTION("request_irq + workqueue bottom-half + latency demo");
