// SPDX-License-Identifier: GPL-2.0
/*
 * day27_edu_loop.c - EDU 循环卸载稳定性测试驱动
 *
 * ==================== 文件概述 ====================
 *
 * Day27 的核心目标：验证驱动在重复 insmod/rmmod 下是否稳定。
 *
 * 【学习焦点】
 *   1. remove 对称性：probe 分配什么，remove 就释放什么，顺序相反
 *   2. 200 次循环测试：验证无资源泄漏、无 oops/panic
 *   3. MSI/LEGACY 回退：pci_alloc_irq_vectors 允许失败时回退
 *   4. 最小化 probe：去掉 Day26 的 identity/liveness 读取，专注稳定性
 *
 * 【与 Day26 的核心区别】
 *   Day26：追求用户态友好接口（read 文本、write 触发、详细错误码）
 *   Day27：追求卸载稳定性（最小化 probe、严格对称 remove、循环测试）
 *
 * 【硬件模型】
 *   与 Day25/Day26 完全相同：QEMU EDU 教学设备 (1234:11e8)
 *   - BAR0 MMIO 映射到寄存器
 *   - MSI 中断（优先）/LEGACY 中断（回退）
 *   - IRQ_RAISE → 触发中断 → IRQ_ACK 清除
 *
 * ==================== 代码结构 ====================
 *
 *  1. 全局资源（模块级别）
 *  2. EDU 寄存器读写封装
 *  3. MSI 中断处理函数
 *  4. 文本状态快照生成
 *  5. file_operations（open/read/write/ioctl）
 *  6. 字符设备注册/销毁
 *  7. PCI probe/remove（对称性关键）
 *  8. 模块 init/exit
 *
 * 【为什么这样组织？】
 *   → 循环测试要求驱动尽量简单，任何冗余代码都可能成为不稳定因素
 *   → 去掉 identity_value/liveness_value 读取，简化验证链路
 *   → 所有资源分配/释放必须严格对称，200 次循环才能稳定
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "include/day27_edu_loop.h"
#include "../include/day27_edu_uapi.h"

/*
 * ==================== 第1部分：全局字符设备资源 ====================
 *
 * 与 Day26 完全相同的全局资源设计。
 *
 * 【为什么需要全局资源？】
 *   → 一个 major + 多个 minor：支持多个 EDU 设备
 *   → class_create：sysfs 类，自动创建设备节点
 *   → atomic_t minor：线程安全的 minor 号分配
 */
static dev_t g_day27_base_dev;
static struct class *g_day27_class;
static atomic_t g_day27_minor = ATOMIC_INIT(0);

/*
 * ==================== 第2部分：EDU 寄存器读写封装 ====================
 *
 * 【与 Day26 的区别】
 *   Day26 注释详细说明了为什么要封装，这里不再重复。
 *   Day27 的封装是 inline 的（static inline），因为函数非常简单，
 *   编译器会内联，减少函数调用开销（对 200 次循环有微小影响）。
 *
 * 【Day27 用到的寄存器】
 *   - IRQ_STATUS (0x24)：读取中断状态
 *   - IRQ_RAISE (0x60)：写入触发中断
 *   - IRQ_ACK (0x64)：写入清除中断
 *
 * 【Day27 没用到但 Day26 用到的寄存器】
 *   - IDENTITY (0x00)：Day26 probe 时读取验证
 *   - LIVENESS (0x04)：Day26 probe 时读取验证
 *   → Day27 简化掉了这些，因为链路已在 Day26 验证通过
 */
static inline u32 day27_read32(struct day27_dev *d, u32 off)
{
    return readl(d->bar0 + off);
}

static inline void day27_write32(struct day27_dev *d, u32 off, u32 val)
{
    writel(val, d->bar0 + off);
}

/*
 * ==================== 第3部分：MSI 中断处理函数 ====================
 *
 * 【与 Day26 的区别】
 *   几乎完全相同。唯一区别是注释更强调"克制"和"可观测性"。
 *
 * 【为什么强调"克制"？】
 *   → 200 次循环中，中断处理函数会被调用 200 次
 *   → 如果 handler 太复杂，会增加延迟和不确定性
 *   → Day27 的 handler 非常简洁：读状态→更新计数→写 ACK→日志
 *
 * 【为什么强调"可观测性"？】
 *   → 每次中断都有 dev_info 日志
 *   → 便于统计中断次数、验证循环是否正常
 *   → 如果某轮循环中断没触发，日志中会明显看到 count 不增长
 */
static irqreturn_t day27_irq_handler(int irq, void *opaque)
{
    struct day27_dev *d = opaque;
    unsigned long flags;
    u32 status;

    /* 第1步：读取中断状态寄存器 */
    status = day27_read32(d, DAY27_EDU_REG_IRQ_STATUS);
    if (!status)
        return IRQ_NONE;  /* 不是我们的中断，快速返回 */

    /* 第2步：更新共享数据（需要加锁保护） */
    spin_lock_irqsave(&d->irq_lock, flags);
    d->irq_count++;
    d->last_irq_status = status;
    d->last_ack_value = status;
    spin_unlock_irqrestore(&d->irq_lock, flags);

    /* 第3步：清除中断（向 IRQ_ACK 写入相同值） */
    day27_write32(d, DAY27_EDU_REG_IRQ_ACK, status);

    /* 第4步：打印日志（供 records 统计和调试） */
    dev_info(&d->pdev->dev,
             "irq handler: irq=%d status=0x%08x count=%llu\n",
             irq, status, d->irq_count);

    return IRQ_HANDLED;
}

/*
 * ==================== 第4部分：生成文本状态快照 ====================
 *
 * 【与 Day26 的区别】
 *   Day26 输出包含 identity_value 和 liveness_inverted，
 *   Day27 简化后不再有这些字段，所以文本快照也更精简。
 *
 * 【输出格式】
 *   vendor=0x1234 device=0x11e8
 *   bar0_start=0x... bar0_len=0x...
 *   irq_vector=XX irq_count=YY msi_enabled=1
 *   last_irq_status=0x... last_ack_value=0x...
 */
static ssize_t day27_build_state_text(struct day27_dev *d, char *buf, size_t size)
{
    return scnprintf(buf, size,
                     "vendor=0x%04x device=0x%04x\n"
                     "bar0_start=0x%llx bar0_len=0x%llx\n"
                     "irq_vector=%u irq_count=%llu msi_enabled=%u\n"
                     "last_irq_status=0x%08x last_ack_value=0x%08x\n",
                     d->pdev->vendor,
                     d->pdev->device,
                     (unsigned long long)d->bar0_start,
                     (unsigned long long)d->bar0_len,
                     d->irq_vector,
                     d->irq_count,
                     !!(d->pdev->msi_enabled),
                     d->last_irq_status,
                     d->last_ack_value);
}

/*
 * ==================== 第5部分：file_operations ====================
 *
 * 【open】
 *   与 Day26 完全相同：通过 container_of 从 inode 获取 day27_dev。
 */
static int day27_open(struct inode *inode, struct file *file)
{
    struct day27_dev *d = container_of(inode->i_cdev, struct day27_dev, cdev);
    file->private_data = d;
    return 0;
}

/*
 * 【read】
 *   与 Day26 完全相同：返回文本状态快照。
 */
static ssize_t day27_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct day27_dev *d = file->private_data;
    char kbuf[192];
    ssize_t len;

    if (!d || !d->bar0)
        return -ENODEV;

    len = day27_build_state_text(d, kbuf, sizeof(kbuf));
    return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

/*
 * 【write】
 *   与 Day26 完全相同：解析整数、验证非零、写入 IRQ_RAISE。
 *
 * 【约束条件】
 *   - 不能为空（count == 0）
 *   - 不能太长（count >= 32）
 *   - 必须能解析为整数
 *   - 必须非零（v == 0 返回 -EINVAL）
 *
 * 【为什么 trigger 0 要报错？】
 *   → 这是故意的负向测试设计
 *   → 验证错误处理路径是否正确
 *   → 避免用户误操作导致无意义的 0 值触发
 */
static ssize_t day27_write(struct file *file, const char __user *buf,
                           size_t count, loff_t *ppos)
{
    struct day27_dev *d = file->private_data;
    char kbuf[32];
    char *end;
    unsigned long v;

    if (!d || !d->bar0)
        return -ENODEV;
    if (count == 0)
        return -EINVAL;
    if (count >= sizeof(kbuf))
        return -E2BIG;
    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;

    kbuf[count] = '\0';
    strim(kbuf);
    if (!kbuf[0])
        return -EINVAL;

    v = simple_strtoul(kbuf, &end, 0);
    if (end == kbuf || *end != '\0')
        return -EINVAL;
    if (v == 0 || v > 0xffffffffUL)
        return -EINVAL;

    dev_info(&d->pdev->dev, "write trigger: value=0x%08lx\n", v);
    day27_write32(d, DAY27_EDU_REG_IRQ_RAISE, (u32)v);
    *ppos += count;
    return count;
}

/*
 * 【ioctl】
 *   Day27 的 ioctl 比 Day26 少一个 GET_IRQ_STATUS，但功能基本相同。
 *
 * 【保留的命令】
 *   - GET_INFO：完整设备信息
 *   - GET_IRQ_COUNT：中断计数
 *   - RESET_STATS：重置计数
 *
 * 【去掉】
 *   - Day26 的 GET_IRQ_STATUS（用 read() 文本状态代替）
 *   → Day27 追求最小化，不需要那么细致的区分
 */
static long day27_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct day27_dev *d = file->private_data;

    switch (cmd) {

    case DAY27_IOC_GET_INFO: {
        struct day27_info info = {
            .tool_api_version = DAY27_TOOL_API_VERSION,
            .vendor_id = d->pdev->vendor,
            .device_id = d->pdev->device,
            .irq_vector = d->irq_vector,
            .irq_count = d->irq_count,
            .last_irq_status = d->last_irq_status,
            .last_ack_value = d->last_ack_value,
            .bar0_start = d->bar0_start,
            .bar0_len = d->bar0_len,
            .msi_enabled = !!(d->pdev->msi_enabled),
        };
        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;
    }

    case DAY27_IOC_GET_IRQ_COUNT: {
        struct day27_irq_count cnt = { .count = d->irq_count };
        if (copy_to_user((void __user *)arg, &cnt, sizeof(cnt)))
            return -EFAULT;
        return 0;
    }

    case DAY27_IOC_RESET_STATS:
        d->irq_count = 0;
        d->last_irq_status = 0;
        d->last_ack_value = 0;
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations day27_fops = {
    .owner = THIS_MODULE,
    .open = day27_open,
    .read = day27_read,
    .write = day27_write,
    .unlocked_ioctl = day27_ioctl,
    .llseek = default_llseek,
};

/*
 * ==================== 第6部分：字符设备注册/销毁 ====================
 *
 * 【setup_chrdev】
 *   与 Day26 完全相同：
 *   1. atomic_fetch_add 分配 minor
 *   2. MKDEV 构建设备号
 *   3. cdev_init + cdev_add
 *   4. device_create 创建设备节点
 *
 * 【destroy_chrdev】
 *   与 Day26 完全相同：
 *   1. device_destroy 销毁节点
 *   2. cdev_del 从 VFS 删除
 */
static int day27_setup_chrdev(struct day27_dev *d)
{
    int ret;
    int minor = atomic_fetch_add(1, &g_day27_minor);

    d->devt = MKDEV(MAJOR(g_day27_base_dev), minor);
    cdev_init(&d->cdev, &day27_fops);
    d->cdev.owner = THIS_MODULE;

    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret)
        return ret;

    d->device = device_create(g_day27_class, &d->pdev->dev, d->devt, d,
                              DAY27_DEV_NAME_FMT, minor);
    if (IS_ERR(d->device)) {
        ret = PTR_ERR(d->device);
        d->device = NULL;
        cdev_del(&d->cdev);
        return ret;
    }
    return 0;
}

static void day27_destroy_chrdev(struct day27_dev *d)
{
    if (d->device)
        device_destroy(g_day27_class, d->devt);
    cdev_del(&d->cdev);
}

/*
 * ==================== 第7部分：PCI probe（最小化版本）====================
 *
 * 【与 Day26 probe 的区别】
 *
 *   Day26 probe 中：
 *   - 读取 IDENTITY 寄存器验证
 *   - 读取 LIVENESS 寄存器验证
 *   - 读取 LIVENESS_INVERTED 计算
 *
 *   Day27 probe 中：
 *   - 去掉以上所有，只保留最小必需操作
 *   - 不再验证 MMIO 映射是否正确（Day26 已验证过）
 *
 * 【为什么简化？】
 *   → 200 次循环中，每轮都做 ID/LIVENESS 验证是多余的
 *   → 简化后的 probe 更快、更稳定、代码更少
 *   → 如果链路有问题，smoke 测试（trigger + count）会捕获
 *
 * 【MSI/LEGACY 回退设计】
 *   ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
 *
 *   → 优先申请 MSI（性能更好）
 *   → 如果 MSI 失败（比如虚拟化环境不支持），自动回退到 LEGACY
 *   → 这样可以兼容更多的测试环境
 */
static int day27_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day27_dev *d;
    int ret;

    dev_info(&pdev->dev, "probe enter: %04x:%04x\n", pdev->vendor, pdev->device);

    /* 第1步：分配私有数据结构 */
    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;
    d->pdev = pdev;

    /* 关联私有数据到 PCI 设备 */
    pci_set_drvdata(pdev, d);

    /* 初始化自旋锁 */
    spin_lock_init(&d->irq_lock);

    /* 第2步：启用 PCI 设备 */
    ret = pci_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "pci_enable_device failed: %d\n", ret);
        goto err_free;
    }

    /* 第3步：请求 BAR 资源 */
    ret = pci_request_regions(pdev, DAY27_DRV_NAME);
    if (ret) {
        dev_err(&pdev->dev, "pci_request_regions failed: %d\n", ret);
        goto err_disable;
    }

    /* 第4步：设置为主设备 */
    pci_set_master(pdev);

    /* 第5步：获取 BAR0 信息 */
    d->bar0_start = pci_resource_start(pdev, 0);
    d->bar0_len   = pci_resource_len(pdev, 0);
    dev_info(&pdev->dev, "BAR0: start=0x%llx len=0x%llx flags=0x%lx\n",
             (unsigned long long)d->bar0_start,
             (unsigned long long)d->bar0_len,
             (unsigned long)pci_resource_flags(pdev, 0));

    /* 第6步：映射 BAR0 MMIO */
    d->bar0 = pci_iomap(pdev, 0, 0);
    if (!d->bar0) {
        ret = -ENOMEM;
        dev_err(&pdev->dev, "BAR0: pci_iomap failed\n");
        goto err_regions;
    }

    /*
     * 第7步：分配中断向量
     *
     * 【关键区别】Day27 使用 MSI|LEGACY 回退
     *   - PCI_IRQ_MSI：优先使用消息信号中断
     *   - PCI_IRQ_LEGACY：如果 MSI 不可用，回退到传统中断
     *   - 第三个参数 1：只分配 1 个向量
     */
    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
    if (ret < 0) {
        dev_err(&pdev->dev, "pci_alloc_irq_vectors failed: %d\n", ret);
        goto err_iounmap;
    }
    d->irq_vector = pci_irq_vector(pdev, 0);

    /* 第8步：注册中断处理函数 */
    ret = request_irq(d->irq_vector, day27_irq_handler, 0, DAY27_DRV_NAME, d);
    if (ret) {
        dev_err(&pdev->dev, "request_irq(%u) failed: %d\n", d->irq_vector, ret);
        goto err_irq_vectors;
    }

    /* 打印 MSI/LEGACY 状态 */
    dev_info(&pdev->dev, "MSI vector=%u enabled=%u\n",
             d->irq_vector, !!pdev->msi_enabled);

    /* 第9步：注册字符设备 */
    ret = day27_setup_chrdev(d);
    if (ret) {
        dev_err(&pdev->dev, "day27_setup_chrdev failed: %d\n", ret);
        goto err_irq;
    }

    dev_info(&pdev->dev, "probe success\n");
    return 0;

/* 错误处理标签（按分配倒序释放） */
err_irq:
    free_irq(d->irq_vector, d);
err_irq_vectors:
    pci_free_irq_vectors(pdev);
err_iounmap:
    pci_iounmap(pdev, d->bar0);
err_regions:
    pci_release_regions(pdev);
err_disable:
    pci_disable_device(pdev);
err_free:
    kfree(d);
    return ret;
}

/*
 * ==================== 第8部分：PCI remove（严格对称）====================
 *
 * 【remove 对称性是 Day27 的核心】
 *
 * probe 分配的资源，必须按倒序在 remove 中释放：
 *
 *   probe:                     remove:
 *   1. kzalloc            →    7. kfree
 *   2. pci_enable_device  →    6. pci_disable_device
 *   3. pci_request_regions →   5. pci_release_regions
 *   4. pci_iomap          →    4. pci_iounmap
 *   5. pci_alloc_irq_vec  →    3. pci_free_irq_vectors
 *   6. request_irq        →    2. free_irq
 *   7. setup_chrdev      →    1. destroy_chrdev
 *
 * 【为什么顺序重要？】
 *   → 一些资源有依赖关系（比如 free_irq 需要 irq_vector）
 *   → 必须先释放后代资源，再释放祖先资源
 *   → 违反顺序会导致 use-after-free 或 double-free
 *
 * 【防御性检查】
 *   → if (!d) return：防止重复 remove
 *   → if (d->bar0) pci_iounmap：bar0 可能映射失败
 *   → if (d->device) device_destroy：device 可能创建失败
 */
static void day27_remove(struct pci_dev *pdev)
{
    struct day27_dev *d = pci_get_drvdata(pdev);

    if (!d)
        return;

    dev_info(&pdev->dev, "remove enter\n");

    /* 第1步：销毁字符设备 */
    day27_destroy_chrdev(d);

    /* 第2步：释放中断处理函数 */
    free_irq(d->irq_vector, d);

    /* 第3步：释放中断向量 */
    pci_free_irq_vectors(pdev);

    /* 第4步：解除 MMIO 映射 */
    if (d->bar0)
        pci_iounmap(pdev, d->bar0);

    /* 第5步：释放 BAR 资源 */
    pci_release_regions(pdev);

    /* 第6步：禁用 PCI 设备 */
    pci_disable_device(pdev);

    /* 第7步：释放私有数据内存 */
    kfree(d);

    dev_info(&pdev->dev, "remove leave\n");
}

/*
 * ==================== 第9部分：pci_driver 定义 ====================
 *
 * 与 Day26 完全相同的 pci_driver 结构体。
 */
static const struct pci_device_id day27_pci_ids[] = {
    { PCI_DEVICE(DAY27_EDU_VENDOR_ID, DAY27_EDU_DEVICE_ID) },
    { }
};
MODULE_DEVICE_TABLE(pci, day27_pci_ids);

static struct pci_driver day27_pci_driver = {
    .name = DAY27_DRV_NAME,
    .id_table = day27_pci_ids,
    .probe = day27_probe,
    .remove = day27_remove,
};

/*
 * ==================== 第10部分：模块 init/exit ====================
 *
 * 与 Day26 完全相同的 init/exit 流程。
 */
static int __init day27_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&g_day27_base_dev, 0, 32, DAY27_DRV_NAME);
    if (ret)
        return ret;

    g_day27_class = class_create(THIS_MODULE, DAY27_CLASS_NAME);
    if (IS_ERR(g_day27_class)) {
        ret = PTR_ERR(g_day27_class);
        unregister_chrdev_region(g_day27_base_dev, 32);
        return ret;
    }

    ret = pci_register_driver(&day27_pci_driver);
    if (ret) {
        class_destroy(g_day27_class);
        unregister_chrdev_region(g_day27_base_dev, 32);
        return ret;
    }

    pr_info("%s: loaded\n", DAY27_DRV_NAME);
    return 0;
}

static void __exit day27_exit(void)
{
    pci_unregister_driver(&day27_pci_driver);
    class_destroy(g_day27_class);
    unregister_chrdev_region(g_day27_base_dev, 32);
    pr_info("%s: unloaded\n", DAY27_DRV_NAME);
}

module_init(day27_init);
module_exit(day27_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Day27 EDU loop/remove symmetry driver");

/*
 * ==================== 附录：200 次循环数据流 ====================
 *
 * 【循环架构】
 *
 *   Guest 用户态                    内核驱动                    EDU 硬件
 *   ─────────────────────────────────────────────────────────────────────
 *
 *   for i in 1..200:
 *       insmod day27_edu_loop.ko
 *           │
 *           ↓
 *       pci_register_driver ────→ day27_probe()
 *           │                        ├→ kzalloc
 *           │                        ├→ pci_enable_device
 *           │                        ├→ pci_request_regions
 *           │                        ├→ pci_iomap
 *           │                        ├→ pci_alloc_irq_vectors(MSI|LEGACY)
 *           │                        ├→ request_irq
 *           │                        └→ day27_setup_chrdev
 *           │
 *       open("/dev/day27_edu0")
 *           │
 *       write("1") ────────────────→ day27_write()
 *           │                             └→ writel(1, IRQ_RAISE)
 *           │                                    │
 *           │                                    ↓
 *           │                               [MSI 中断]
 *           │                                    │
 *           │              day27_irq_handler() ←─┘
 *           │                    ├→ irq_count++
 *           │                    └→ writel(status, IRQ_ACK)
 *           │
 *       ioctl(GET_IRQ_COUNT) ───→ irq_count > 0 ? pass : fail
 *           │
 *       rmmod day27_edu_loop
 *           │
 *           ↓
 *       pci_unregister_driver ───→ day27_remove()
 *                                        ├→ destroy_chrdev
 *                                        ├→ free_irq
 *                                        ├→ pci_free_irq_vectors
 *                                        ├→ pci_iounmap
 *                                        ├→ pci_release_regions
 *                                        ├→ pci_disable_device
 *                                        └→ kfree
 *
 *   loop-summary.txt:
 *       loop_count=200
 *       pass=200
 *       fail=0
 *
 * 【关键观测点】
 *
 *   dmesg 中应该反复出现（每轮 2 次）：
 *       "probe enter: 1234:11e8"
 *       "probe success"
 *       "irq handler: irq=XX status=0x... count=YY"  ← 每轮至少 1 次
 *       "remove enter"
 *       "remove leave"
 *
 *   如果失败，dmesg 中会出现：
 *       "BUG:" / "Oops:" / "Kernel panic" / "hung task"
 */
