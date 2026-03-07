#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/types.h>

/*
 * Day08 主题
 * platform_driver probe/remove + devm 资源管理
 *
 * 这一版有意不创建 /dev 节点
 * 目的就是把 platform 总线这条线单独讲清楚
 *
 * 前面 W1 的字符设备重点是
 *   1. 自己注册 cdev
 *   2. 自己提供 file_operations
 *   3. 用户态通过 /dev/demo 和驱动交互
 *
 * 这一版 W2 的 platform_driver 重点是
 *   1. 设备对象从哪里来
 *   2. 驱动怎么和设备匹配
 *   3. 资源怎么从 platform_device 取出来
 *   4. probe/remove 的生命周期如何工作
 *   5. devm_* 为什么能简化资源回收
 *
 * 也就是说
 * 字符设备那条线解决的是“如何暴露用户态接口”
 * platform_driver 这条线解决的是“驱动如何绑定一个设备并管理它的资源”
 */

#define DRV_NAME "demo_pdrv"

/*
 * 这份 platform_data 只是教学用
 *
 * 真实板级开发里平台数据可能来自两种来源
 *   1. 老式板文件 board file
 *   2. Device Tree
 *
 * Day08 先故意保留 platform_data
 * 这样你能先把 probe 的基本思路学清楚
 * Day09 再把“设备描述来源”切换到 DT
 */
struct demo_platdata {
    u32 version;
    const char *label;
};

/*
 * 每个被 probe 成功的设备都可以有一份私有数据
 *
 * 这和前面字符设备里的 struct demo_device 很像
 * 只是这里的私有数据是“挂在 platform_device 上”
 * 而不是“挂在 inode->i_cdev 上”
 */
struct demo_priv {
    struct device *dev;
    struct resource *mem;
    int irq;
    u32 version;
    const char *label;
};

/*
 * devm_add_action_or_reset 的回调
 *
 * 为什么要专门加这个回调
 * 因为很多初学者只知道 devm_kzalloc 会自动释放内存
 * 但对“设备解绑时 devres 框架还会顺序执行清理动作”没有直观感受
 *
 * 这里故意加一个可见日志
 * 这样在 rmmod 时你就能明确看到
 *   remove
 *   devm cleanup
 *   platform_device release
 * 这三步的先后关系
 */
static void demo_devm_cleanup(void *data)
{
    struct demo_priv *priv = data;

    dev_info(priv->dev,
             "devm cleanup: label=%s version=%u irq=%d\n",
             priv->label ? priv->label : "none",
             priv->version,
             priv->irq);
}

/*
 * probe 是 platform_driver 最重要的入口
 *
 * 什么时候会进入 probe
 *   1. 某个 platform_device 已经存在
 *   2. 某个 platform_driver 被注册到 platform bus
 *   3. 两者匹配成功
 *
 * 匹配成功后内核调用 probe
 * probe 的职责通常是
 *   1. 取资源
 *   2. 分配私有数据
 *   3. 初始化硬件或软件状态
 *   4. 申请中断
 *   5. 注册子功能接口
 *
 * 当前 Day08 刻意只做“取资源 + 打印 + devm 管理”
 * 不做真实 MMIO 访问
 * 这样更容易把主线看清楚
 */
static int demo_probe(struct platform_device *pdev)
{
    struct demo_priv *priv;
    struct resource *mem;
    int irq;
    const struct demo_platdata *pdata;

    dev_info(&pdev->dev, "probe start\n");

    /*
     * devm_kzalloc 的核心价值
     * 内存的生命周期跟着这个 device 走
     * 设备解绑时会自动释放
     * probe 中途失败时也不需要你手工 kfree
     */
    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->dev = &pdev->dev;

    /*
     * 取第 0 个 MEM 资源
     *
     * 前面字符设备主要自己维护缓冲区和状态
     * platform 驱动则更像是“先从设备描述里把资源领出来”
     * 常见资源包括
     *   1. MEM
     *   2. IRQ
     *   3. DMA
     *   4. clock
     *   5. reset
     *   6. regulator
     */
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!mem) {
        dev_err(&pdev->dev, "no MEM resource found\n");
        return -ENODEV;
    }
    priv->mem = mem;

    /*
     * 取第 0 个 IRQ 资源
     *
     * 这里只是把 IRQ 号取出来并打印
     * Day09 再进入 request_irq 或 devm_request_irq 的实战
     */
    irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
        dev_err(&pdev->dev, "no IRQ resource found: %d\n", irq);
        return irq;
    }
    priv->irq = irq;

    /*
     * 取平台数据
     *
     * 这是传统 platform_device 常见写法
     * 当前只是为了教学
     * 让你先理解“probe 不只是靠 /dev 节点工作”
     */
    pdata = dev_get_platdata(&pdev->dev);
    if (pdata) {
        priv->version = pdata->version;
        priv->label = pdata->label;
    } else {
        priv->version = 0;
        priv->label = "no-pdata";
    }

    /*
     * 把私有数据挂到 pdev 上
     * remove 时可以取回来
     */
    platform_set_drvdata(pdev, priv);

    dev_info(&pdev->dev,
             "MEM resource: start=0x%llx end=0x%llx size=0x%llx\n",
             (unsigned long long)mem->start,
             (unsigned long long)mem->end,
             (unsigned long long)resource_size(mem));

    dev_info(&pdev->dev, "IRQ resource: %d\n", irq);
    dev_info(&pdev->dev, "platform_data: version=%u label=%s\n",
             priv->version,
             priv->label);

    /*
     * 这里故意再挂一个 devm action
     * 不是为了功能需要
     * 而是为了让 remove 之后的自动清理顺序在 dmesg 中肉眼可见
     */
    return devm_add_action_or_reset(&pdev->dev, demo_devm_cleanup, priv);
}

/*
 * remove 是 probe 的反向路径
 *
 * 常见职责包括
 *   1. 注销子设备
 *   2. 停止硬件
 *   3. 关闭中断
 *   4. 让设备回到安全状态
 *
 * 但 devm 托管的资源通常不需要你在这里手工释放
 * 这就是 devm 让代码更稳的地方
 */
static int demo_remove(struct platform_device *pdev)
{
    struct demo_priv *priv = platform_get_drvdata(pdev);

    dev_info(&pdev->dev,
             "remove: label=%s version=%u irq=%d\n",
             priv->label ? priv->label : "none",
             priv->version,
             priv->irq);

    /*
     * 这里没有 kfree(priv)
     * 因为 priv 是 devm_kzalloc 分配的
     * 解绑时内核会自动释放
     */
    return 0;
}

/*
 * platform_driver 的核心对象
 *
 * .driver.name 是最基础的匹配关键字
 * 当前 Day08 用“同名匹配”来触发 probe
 *
 * 后面切到 Device Tree 后
 * 还会引入 of_match_table
 */
static struct platform_driver demo_platform_driver = {
    .probe  = demo_probe,
    .remove = demo_remove,
    .driver = {
        .name = DRV_NAME,
    },
};

/*
 * 下面这组资源是“教学用假资源”
 *
 * 注意
 * 当前只是为了演示 platform_get_resource 的使用方式
 * 并不访问这段地址
 * 也不申请真实中断
 *
 * 这样设计的原因是
 * Day08 的学习目标是先把 platform bus 的绑定和资源模型讲清楚
 * 不是马上做硬件寄存器读写
 */
static struct resource demo_resources[] = {
    DEFINE_RES_MEM(0x10000000, 0x1000),
    DEFINE_RES_IRQ(11),
};

static struct demo_platdata demo_pdata = {
    .version = 1,
    .label   = "w2-day08-platform-lab",
};

/*
 * 每个 device 最终都需要 release 回调
 *
 * 这是 device model 里非常重要但很容易被忽略的一点
 * 如果 release 缺失
 * 内核会给出警告
 *
 * 这里的 release 不等于 remove
 * remove 是“解绑驱动”
 * release 更接近“底层 device 对象生命周期真正结束”
 */
static void demo_pdev_release(struct device *dev)
{
    pr_info(DRV_NAME ": platform_device release\n");
}

/*
 * Day08 为了让 QEMU 里的实验尽量自给自足
 * 直接在模块中注册一个教学用 platform_device
 *
 * 这样 insmod 时就能得到完整链路
 *   platform_device_register
 *   platform_driver_register
 *   match
 *   probe
 *
 * 真正板级开发里这个 device 往往不是驱动自己注册的
 * 而是来自板文件或者 Device Tree
 */
static struct platform_device demo_platform_device = {
    .name          = DRV_NAME,
    .id            = -1,
    .num_resources = ARRAY_SIZE(demo_resources),
    .resource      = demo_resources,
    .dev = {
        .platform_data = &demo_pdata,
        .release       = demo_pdev_release,
    },
};

/*
 * 模块初始化入口
 *
 * 这里特意不用 module_platform_driver 宏
 * 因为我们除了注册 driver 之外
 * 还要先注册一个配套的教学用 device
 */
static int __init demo_init(void)
{
    int ret;

    pr_info(DRV_NAME ": module init\n");

    /*
     * 先注册 device
     * 后注册 driver
     *
     * 当 driver 注册到 platform bus 时
     * 内核会扫描已存在的同名设备
     * 匹配成功后触发 probe
     */
    ret = platform_device_register(&demo_platform_device);
    if (ret) {
        pr_err(DRV_NAME ": platform_device_register failed: %d\n", ret);
        return ret;
    }

    ret = platform_driver_register(&demo_platform_driver);
    if (ret) {
        pr_err(DRV_NAME ": platform_driver_register failed: %d\n", ret);
        platform_device_unregister(&demo_platform_device);
        return ret;
    }

    pr_info(DRV_NAME ": module init ok\n");
    return 0;
}

/*
 * 模块退出入口
 *
 * 注销顺序也值得观察
 *   1. platform_driver_unregister
 *      如果当前 device 仍绑定着这个 driver
 *      内核会先走 remove
 *      然后触发 devm 管理资源的释放
 *
 *   2. platform_device_unregister
 *      最终触发 device release
 *
 * dmesg 里能看到完整顺序
 */
static void __exit demo_exit(void)
{
    pr_info(DRV_NAME ": module exit\n");

    platform_driver_unregister(&demo_platform_driver);
    platform_device_unregister(&demo_platform_device);
}

module_init(demo_init);
module_exit(demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richer Wong");
MODULE_DESCRIPTION("W2D1 Day08 platform_driver + devm resource management demo");
