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
#include <linux/irqdesc.h>

/*
 * Day10 的核心目标：
 * 1. 继续沿用 day09 的 DT platform 设备实验框架
 * 2. 在 probe() 中通过 platform_get_irq() 取到 Linux virq
 * 3. 使用 request_irq() 注册一个最小 top-half handler
 * 4. 通过 /proc/demo_irq_trigger 做软件注入，观察 handler 被执行
 * 5. 用 /proc/interrupts 和 /proc/demo_irq_stats 双重验证计数变化
 *
 * 仍然不是一个“真实硬件驱动”，而是一个教学型最小 IRQ demo。
 *
 * 当前 DT 里描述的是一个 fake 设备节点：
 * - DT 中有 reg / interrupts
 * - 内核可以把 interrupts 翻译为 Linux IRQ 但 QEMU virt 并没有为它接入真实会跳变的外设
 *
 * 如果只 request_irq() 而没有额外触发手段，计数通常不会自己增长
 * 为此这里专门加了一个 proc 写接口，主动对已申请成功的 virq 做软件注入
 */
#define DRV_NAME          "demo_irq"
#define PROC_STATS_NAME   "demo_irq_stats"
#define PROC_TRIGGER_NAME "demo_irq_trigger"
#define PROC_BUF_SZ       32

/*
 * day09 更关注：
 * - DT 里的 reg / interrupts 原始内容是什么
 * - platform 层把它们解析成了什么资源
 *
 * day10 在此基础上继续加入：
 * - Linux virq 编号
 * - 中断触发次数
 * - proc 观察接口
 * 在一个结构体把 “原始描述 -> 解析结果 -> 运行时统计” 串起来
 */
struct demo_irq_priv {
    
    struct device *dev;                    /* 关联设备对象，便于打印 dev_info/dev_err 日志 */
    struct resource mem;                   /* 由 platform_get_resource() 解析出的内存资源 */
    int linux_irq;                         /*platform_get_irq() 解析出的 Linux IRQ 编号（virq）*/
    u32 raw_reg[4];                        /* 保存 DT 中 reg 属性的原始 cells，方便教学对照 */
    u32 raw_irq[3];                        /* 保存 DT 中 interrupts 属性的原始 cells，方便教学对照 */
    const char *label;                     /* 从 DT 自定义属性 demo,label 读到的教学标签 */
    const char *match_name;                /* of_match_table 中 data 字段携带的匹配信息 */
    atomic64_t irq_count;                  /*累计中断次数, 使用 atomic64_t 因为handler 运行在中断上下文中，计数时最适合用原子变量*/
    struct proc_dir_entry *proc_stats;     /* /proc/demo_irq_stats */
    struct proc_dir_entry *proc_trigger;   /* /proc/demo_irq_trigger */
};

/*
 * 教学简化：当前 demo 只支持一个设备实例。
 * /proc 读写接口拿不到 pdev，所以这里保留一个全局指针，把当前 demo 实例的私有数据带到 proc 接口里。
 * 这不是多实例场景下最严谨的设计，但对单实例教学 demo 来说更直观。
 */
static struct demo_irq_priv *g_demo_priv;

/*
 * 读取 DT 中 reg 属性的原始 cells。
 * 目的不是驱动必须这样做，而是为了教学：
 * - 让你看到 reg 在 DT 里原始长什么样 再对照 platform_get_resource() 解析后的 struct resource
 */
static void demo_irq_dump_raw_reg(struct device *dev,
                                  struct device_node *np,
                                  struct demo_irq_priv *priv)
{
    int ret;

    ret = of_property_read_u32_array(np, "reg", priv->raw_reg, ARRAY_SIZE(priv->raw_reg));
    if (ret) {
        dev_warn(dev, "raw DT reg read failed: %d\n", ret);
        return;
    }
    dev_info(dev, "raw DT reg cells: <%#x %#x %#x %#x>\n", priv->raw_reg[0], priv->raw_reg[1], priv->raw_reg[2], priv->raw_reg[3]);
}

/*
 * 读取 DT 中 interrupts 属性的原始 cells。
 * 可以同时看到：
 * - DT 里写的中断三元组
 * - platform_get_irq() 最终给出的 Linux virq
 */
static void demo_irq_dump_raw_irq(struct device *dev,
                                  struct device_node *np,
                                  struct demo_irq_priv *priv)
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

/*
 * 最小 top-half 处理函数
 *
 * 这节课的目标不是实现真实硬件 ACK，也不是上来就讲 bottom half，而是先把“request_irq 成功之后，handler 什么时候会跑”这件事看明白
 * - 只做计数
 * - 打一条限速日志
 * - 返回 IRQ_HANDLED
 */
static irqreturn_t demo_irq_handler(int irq, void *dev_id)
{
    struct demo_irq_priv *priv = dev_id;
    s64 new_count;

    new_count = atomic64_inc_return(&priv->irq_count);

    /*
     * 用 ratelimited 避免连续触发时刷爆日志。
     * 如果一次 echo 100、1000，这里不会疯狂打印。
     */
    dev_info_ratelimited(priv->dev, "top-half handled irq=%d count=%lld\n", irq, (long long)new_count);
    return IRQ_HANDLED;
}

/*
 * /proc/demo_irq_stats 的显示函数 接口代表 驱动内部视角：
 * - 当前模块名
 * - 当前标签
 * - 当前 linux_irq
 * - 当前 irq_count
 * - 当前 mem resource
 *
 * 与 /proc/interrupts 结合着看，最容易建立直觉。
 */
static int demo_irq_stats_show(struct seq_file *m, void *v)
{
    struct demo_irq_priv *priv = g_demo_priv;

    if (!priv) {
        seq_puts(m, "driver not ready\n");
        return 0;
    }

    seq_printf(m, "module=%s\n", DRV_NAME);
    seq_printf(m, "label=%s\n", priv->label ? priv->label : "none");
    seq_printf(m, "linux_irq=%d\n", priv->linux_irq);
    seq_printf(m, "irq_count=%lld\n", (long long)atomic64_read(&priv->irq_count));
    seq_printf(m, "mem_start=0x%llx\n", (unsigned long long)priv->mem.start);
    seq_printf(m, "mem_size=0x%llx\n", (unsigned long long)resource_size(&priv->mem));

    return 0;
}

static int demo_irq_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, demo_irq_stats_show, NULL);
}

/*
 * /proc/demo_irq_trigger 写接口。
 * 这里不是“真实硬件拉高中断线”，而是从进程上下文中，主动对一条已经 request_irq 成功的 Linux virq 做软件注入。
 *
 * 用法：
 *   echo 1  > /proc/demo_irq_trigger
 *   echo 10 > /proc/demo_irq_trigger
 *
 * 效果：
 *   demo_irq_trigger_write()
 *       -> generic_handle_irq(linux_irq)
 *       -> demo_irq_handler()
 *       -> irq_count 增长
 *
 * 这样在没有真实外设的条件下，也能把 IRQ 注册到 handler 执行的路径观察完整。
 */
static ssize_t demo_irq_trigger_write(struct file *file,
                                      const char __user *buf,
                                      size_t count,
                                      loff_t *ppos)
{
    struct demo_irq_priv *priv = g_demo_priv;
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

    /*
     * 教学接口做个上限保护
     * n == 0 没有意义；n 太大则会让日志和统计都不容易观察，也可能导致误操作。
     */
    if (n == 0 || n > 100000)
        return -EINVAL;

    for (i = 0; i < n; i++) {
        ret = generic_handle_irq(priv->linux_irq);
        if (ret) {
            dev_err(priv->dev, "generic_handle_irq failed at iter=%u ret=%d\n", i, ret);
            return ret;
        }
    }

    dev_info(priv->dev, "soft-trigger injected %u times irq_count=%lld\n",
             n, (long long)atomic64_read(&priv->irq_count));

    return count;
}

/*
 * /proc/demo_irq_stats 的文件操作集
 *
 * 读 stats 节点时会进入 single_open -> demo_irq_stats_show，然后通过 seq_file 框架把内部统计打印出来。
 */
static const struct proc_ops demo_irq_stats_fops = {
    .proc_open    = demo_irq_stats_open, /* 打开 /proc 节点时建立 single_open 上下文 */
    .proc_read    = seq_read,            /* 读取具体内容，由 seq_file 框架驱动 */
    .proc_lseek   = seq_lseek,           /* 支持常规偏移移动 */
    .proc_release = single_release,      /* 关闭文件时释放 single_open 相关资源 */
};

/*
 * /proc/demo_irq_trigger 的文件操作集。
 *
 * 这里只需要写接口，因为这个节点的职责就是接受用户态 echo N，
 * 然后在内核里循环调用 generic_handle_irq() 注入软件中断。
 */
static const struct proc_ops demo_irq_trigger_fops = {
    .proc_write = demo_irq_trigger_write, /* 写入触发次数，执行软注入 */
};

/*
 * proc 节点的创建和删除拆开写
 * 这样 probe/remove 的逻辑更清晰：
 * - probe 里只负责 创建成功或回滚
 * - remove 里只负责 统一清理
 */
static void demo_irq_remove_proc(struct demo_irq_priv *priv)
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

static int demo_irq_create_proc(struct demo_irq_priv *priv)
{
    priv->proc_stats = proc_create(PROC_STATS_NAME, 0444, NULL,
                                   &demo_irq_stats_fops);
    if (!priv->proc_stats)
        return -ENOMEM;

    priv->proc_trigger = proc_create(PROC_TRIGGER_NAME, 0222, NULL, &demo_irq_trigger_fops);
    if (!priv->proc_trigger) {
        demo_irq_remove_proc(priv);
        return -ENOMEM;
    }

    return 0;
}

/*
 * of_match_table：
 * 当 DT 里出现 compatible = "demo,irq-pdrv" 的节点时，
 * platform 总线会尝试把它和这个驱动匹配起来。
 */
static const struct of_device_id demo_irq_match[] = {
    {
        .compatible = "demo,irq-pdrv", /* 只要 DT 节点 compatible 命中，就会尝试绑定本驱动 */
        .data = "irq-of-match",        /* 给 probe 读取的匹配私有数据，方便教学观察 */
    },
    { }                                 /* sentinel：匹配表结束 */
};
MODULE_DEVICE_TABLE(of, demo_irq_match);

/**
1. probe() 的核心任务
   1. 激活硬件：从设备树（DT）或 ACPI 获取寄存器基地址、中断号（IRQ）、时钟（Clock）和电源管理（PM）资源。
   2. 分配内存：为驱动私有数据结构（通常是 struct my_device_data）分配内存。
   3. 映射寄存器：使用 ioremap 或 devm_regmap_init 将物理地址映射到内核虚拟地址空间。
   4. 注册中断：设置 Top-half（中断处理函数）并准备下半部机制。
   5. 提供接口：向内核子系统注册设备（如 input_register_device 或 register_netdev），使用户层可见。

2. 编写 probe() 的现代准则
* 使用 devm_ 系列函数：如 devm_kzalloc、devm_request_irq。这样在驱动卸载或 probe 失败时，内核会自动释放资源，避免复杂的错误处理跳转（goto 标签）
* 检查返回值：必须检查每一个资源申请的结果。如果硬件没准备好，返回 -EPROBE_DEFER 让内核稍后重试。

3. 与 Regmap 的联动
    在 probe() 中初始化 Regmap 是标准做法：

    static int my_driver_probe(struct i2c_client *client) {
        struct regmap *map;
        // 1. 初始化 Regmap 封装寄存器
        map = devm_regmap_init_i2c(client, &my_regmap_config);
        if (IS_ERR(map)) return PTR_ERR(map);

        // 2. 此时 DebugFS 已经自动创建了对应的寄存器快照节点
        // 3. 读取 ID 寄存器确认硬件存在
        regmap_read(map, REG_ID, &val);
        ...
    }

4. 触发时机
* 系统启动时：内核扫描总线发现匹配的节点。
* 热插拔时：插入 USB 或 SDIO 设备。
* 手动加载：执行 insmod 加载驱动模块时。
*/


/*
 * 执行顺序可以概括为：
 * 1. 确认设备来自 DT
 * 2. 分配并初始化私有数据
 * 3. 读取教学属性和原始 DT cells
 * 4. 解析 MEM resource
 * 5. 解析 Linux virq
 * 6. request_irq() 注册 top-half
 * 7. 创建 proc 观察接口
 * 8. 保存 drvdata / 全局入口
 */
static int demo_irq_probe(struct platform_device *pdev)
{
    struct demo_irq_priv *priv;
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
    atomic64_set(&priv->irq_count, 0);
    priv->match_name = (const char *)device_get_match_data(dev);

    ret = of_property_read_string(np, "demo,label", &priv->label);
    if (ret)
        priv->label = "no-label";

    dev_info(dev, "of node full name: %s\n", np->full_name);
    dev_info(dev, "of match data: %s\n", priv->match_name ? priv->match_name : "none");
    dev_info(dev, "dt label: %s\n", priv->label);

    //打印 DT 原始内容
    demo_irq_dump_raw_reg(dev, np, priv);
    demo_irq_dump_raw_irq(dev, np, priv);

    /*
     * 解析出平台设备的内存资源
     * 这里没有真的去 ioremap/访问寄存器，但保留这一步是为了维持完整的platform driver 学习路径：reg -> resource
     */
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

    //解析平台设备的第 0 个中断资源。返回的是 Linux 内部使用的 virq 编号
    irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
        dev_err(dev, "platform_get_irq failed: %d\n", irq);
        return irq;
    }

    priv->linux_irq = irq;
    dev_info(dev, "parsed Linux IRQ: %d\n", priv->linux_irq);

    //申请中断, 这条 virq 已经有了 top-half handler，后面只要这条 IRQ 被触发，就会进入 demo_irq_handler()
    ret = request_irq(priv->linux_irq, demo_irq_handler, 0, DRV_NAME, priv);
    if (ret) {
        dev_err(dev, "request_irq(%d) failed: %d\n", priv->linux_irq, ret);
        return ret;
    }

    /*
     * 创建两个教学 proc 接口：
     * - stats   : 读取内部状态
     * - trigger : 软件触发 IRQ
     */
    ret = demo_irq_create_proc(priv);
    if (ret) {
        dev_err(dev, "create proc entries failed: %d\n", ret);
        free_irq(priv->linux_irq, priv);
        return ret;
    }

    platform_set_drvdata(pdev, priv);
    g_demo_priv = priv;

    dev_info(dev, "request_irq done trigger via: echo 1 > /proc/%s\n", PROC_TRIGGER_NAME);
    dev_info(dev, "verify via: cat /proc/interrupts | grep %s\n", DRV_NAME);

    return 0;
}

static int demo_irq_remove(struct platform_device *pdev)
{
    struct demo_irq_priv *priv = platform_get_drvdata(pdev);

    //先让 proc 接口不再能访问到这份实例，再做后续清理。
    g_demo_priv = NULL;

    demo_irq_remove_proc(priv);
    free_irq(priv->linux_irq, priv);

    dev_info(&pdev->dev, "remove: linux_irq=%d irq_count=%lld label=%s\n",
             priv->linux_irq, (long long)atomic64_read(&priv->irq_count),
             priv->label ? priv->label : "none");

    return 0;
}

static struct platform_driver demo_irq_driver = {
    .probe = demo_irq_probe,         /* 匹配成功后的入口：完成 DT 解析、request_irq、/proc 创建 */
    .remove = demo_irq_remove,       /* 设备解绑/模块卸载时的出口：删除 /proc、释放 IRQ */
    .driver = {
        .name = DRV_NAME,            /* 驱动名字，供 platform 总线、sysfs 和日志使用 */
        .of_match_table = demo_irq_match,
                                     /* OF 匹配表：决定 compatible 命中的节点由谁来管理 */
    },
};

static int __init demo_irq_init(void)
{
    pr_info(DRV_NAME ": module init\n"); /* 模块入口：先把 platform_driver 注册进内核 */
    return platform_driver_register(&demo_irq_driver);
    /* 注册后如果已经存在匹配的 DT 节点，就会进一步进入 demo_irq_probe() */
}

static void __exit demo_irq_exit(void)
{
    pr_info(DRV_NAME ": module exit\n"); /* 模块退出：准备从 platform 总线注销驱动 */
    platform_driver_unregister(&demo_irq_driver);
    /* 注销驱动；若设备仍绑定，会先触发 remove() */
}

module_init(demo_irq_init); 
module_exit(demo_irq_exit);

MODULE_LICENSE("GPL");   
MODULE_AUTHOR("Richer Wong");
MODULE_DESCRIPTION("request_irq top-half soft-trigger demo");
