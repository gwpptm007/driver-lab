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

/*
 * Day09 主题
 * Device Tree + of_match_table + reg/irq 解析
 *
 * 这一版和 Day08 最核心的区别是：
 * Day08 的设备对象是模块里手工注册出来的 platform_device
 * Day09 的设备对象来自 Device Tree
 *
 * 也就是说，驱动自己不再“造一个教学用设备”
 * 而是改成：
 *   1. 启动时由 DT 描述设备
 *   2. 内核根据 DT 生成 platform_device
 *   3. 模块加载后由 of_match_table 进行匹配
 *   4. probe 中解析并打印 reg / irq
 *
 * 这一课故意仍然不申请真实 IRQ，也不访问 MMIO
 * 目标是先把“DT -> platform_device -> platform_driver -> probe”这条链跑顺
 * Day10 再继续接 request_irq 和 regmap
 */

#define DRV_NAME "demo_of_pdrv"

/*
 * 私有数据仍然保留
 *
 * 前面 Day08 你已经看过 platform_set_drvdata 和 platform_get_drvdata
 * Day09 继续沿用这套结构
 * 只是数据来源从“手工资源数组”换成了“DT 节点里的属性”
 */
struct demo_of_priv {
    struct device *dev;
    struct resource mem;
    int linux_irq;
    u32 raw_reg[4];
    u32 raw_irq[3];
    const char *label;
    const char *match_name;
};

/*
 * 这个清理动作不是功能必需
 * 主要还是为了让你在 dmesg 里看到 remove 之后 devm 的自动清理顺序
 */
static void demo_of_devm_cleanup(void *data)
{
    struct demo_of_priv *priv = data;

    dev_info(priv->dev,
             "devm cleanup: label=%s match=%s linux_irq=%d\n",
             priv->label ? priv->label : "none",
             priv->match_name ? priv->match_name : "none",
             priv->linux_irq);
}

/*
 * 把 reg 原始 cells 打印出来
 *
 * 对 arm64 的 QEMU virt 来说
 * 根节点通常使用 2 个 address cells + 2 个 size cells
 * 所以一个最简单的 reg 会是 4 个 u32：
 *   <addr_hi addr_lo size_hi size_lo>
 */
static void demo_of_dump_raw_reg(struct device *dev,
                                 struct device_node *np,
                                 struct demo_of_priv *priv)
{
    int ret;

    ret = of_property_read_u32_array(np, "reg", priv->raw_reg, ARRAY_SIZE(priv->raw_reg));
    if (ret) {
        dev_warn(dev, "raw DT reg read failed: %d\n", ret);
        return;
    }

    dev_info(dev,
             "raw DT reg cells: <%#x %#x %#x %#x>\n",
             priv->raw_reg[0],
             priv->raw_reg[1],
             priv->raw_reg[2],
             priv->raw_reg[3]);
}

/*
 * 把 interrupts 原始 cells 打印出来
 *
 * QEMU virt 上的 GIC 中断描述常见格式是 3 个 cell：
 *   <type irq flags>
 * 例如：
 *   <0 42 4>
 *
 * 这里先打印原始 DT 值
 * 后面再通过 platform_get_irq 看内核映射后的 Linux IRQ 号
 */
static void demo_of_dump_raw_irq(struct device *dev,
                                 struct device_node *np,
                                 struct demo_of_priv *priv)
{
    int ret;

    ret = of_property_read_u32_array(np, "interrupts", priv->raw_irq, ARRAY_SIZE(priv->raw_irq));
    if (ret) {
        dev_warn(dev, "raw DT interrupts read failed: %d\n", ret);
        return;
    }

    dev_info(dev,
             "raw DT interrupts cells: <%#x %#x %#x>\n",
             priv->raw_irq[0],
             priv->raw_irq[1],
             priv->raw_irq[2]);
}

/*
 * of_match_table 的 data 字段只是教学辅助
 *
 * 这样你在 probe 里不仅能知道 compatible 匹配成功了
 * 还能顺便看到一份和 match 项关联的数据
 */
static const struct of_device_id demo_of_match[] = {
    {
        .compatible = "demo,day09-pdrv",
        .data = "day09-of-match",
    },
    {
        /* sentinel */
    }
};
MODULE_DEVICE_TABLE(of, demo_of_match);

/*
 * probe 入口
 *
 * 这一版和 Day08 一样，核心还是：
 *   1. 分配私有数据
 *   2. 获取资源
 *   3. 打印资源
 *   4. 保存 drvdata
 *
 * 只是资源来源变成了 DT 节点
 */
static int demo_of_probe(struct platform_device *pdev)
{
    struct demo_of_priv *priv;
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
    /*
     * Linux 5.15 上优先使用通用的 device_get_match_data，
     * 这样对 OF 匹配和后续扩展都更稳妥。
     */
    //priv->match_name = of_device_get_match_data(dev);
    priv->match_name = (const char *)device_get_match_data(dev);

    /*
     * 读取一个自定义字符串属性
     * 这样你能看到 probe 不只会解析 reg / interrupts
     * 也能顺手取普通 DT 属性
     */
    ret = of_property_read_string(np, "demo,label", &priv->label);
    if (ret)
        priv->label = "no-label";

    dev_info(dev, "of node full name: %s\n", np->full_name);
    dev_info(dev, "of match data: %s\n", priv->match_name ? priv->match_name : "none");
    dev_info(dev, "dt label: %s\n", priv->label);

    demo_of_dump_raw_reg(dev, np, priv);
    demo_of_dump_raw_irq(dev, np, priv);

    /*
     * platform_get_resource 的使用方式并没有因为上了 DT 就改变
     *
     * 差别在于：
     * Day08 的 resource 来自手工 platform_device
     * Day09 的 resource 来自 DT 中的 reg，经内核翻译后挂到 pdev 上
     */
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!mem) {
        dev_err(dev, "no MEM resource parsed from DT\n");
        return -ENODEV;
    }

    memcpy(&priv->mem, mem, sizeof(*mem));

    dev_info(dev,
             "parsed MEM resource: start=0x%llx end=0x%llx size=0x%llx\n",
             (unsigned long long)priv->mem.start,
             (unsigned long long)priv->mem.end,
             (unsigned long long)resource_size(&priv->mem));

    /*
     * platform_get_irq 会在 DT 中解析 interrupts 属性
     * 再走 irq domain 映射，返回 Linux IRQ 号
     *
     * 这里返回的是 Linux 里的 virq
     * 不一定和 DT 里写的原始中断号完全一样
     */
    irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
        dev_err(dev, "platform_get_irq failed: %d\n", irq);
        return irq;
    }

    priv->linux_irq = irq;
    dev_info(dev, "parsed Linux IRQ: %d\n", priv->linux_irq);

    platform_set_drvdata(pdev, priv);

    return devm_add_action_or_reset(dev, demo_of_devm_cleanup, priv);
}

/*
 * remove 依然很简单
 * 因为当前并没有申请真实 IRQ，也没有 ioremap 寄存器
 * 重点仍然是看解绑和 devm 自动清理顺序
 */
static int demo_of_remove(struct platform_device *pdev)
{
    struct demo_of_priv *priv = platform_get_drvdata(pdev);

    dev_info(&pdev->dev,
             "remove: label=%s match=%s linux_irq=%d mem_start=0x%llx\n",
             priv->label ? priv->label : "none",
             priv->match_name ? priv->match_name : "none",
             priv->linux_irq,
             (unsigned long long)priv->mem.start);

    return 0;
}

/*
 * 这里最关键的是 of_match_table
 *
 * Day08 的匹配依据主要是 driver.name == device.name
 * Day09 的匹配依据主要是 DT compatible 和 of_match_table
 */
static struct platform_driver demo_of_driver = {
    .probe = demo_of_probe,
    .remove = demo_of_remove,
    .driver = {
        .name = DRV_NAME,
        .of_match_table = demo_of_match,
    },
};

static int __init demo_of_init(void)
{
    pr_info(DRV_NAME ": module init\n");
    return platform_driver_register(&demo_of_driver);
}

static void __exit demo_of_exit(void)
{
    pr_info(DRV_NAME ": module exit\n");
    platform_driver_unregister(&demo_of_driver);
}

module_init(demo_of_init);
module_exit(demo_of_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richer Wong");
MODULE_DESCRIPTION("Day09 Device Tree reg/irq parse demo");
