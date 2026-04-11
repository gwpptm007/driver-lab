// SPDX-License-Identifier: GPL-2.0
/*
 * Day25 - EDU MSI interrupt experiment
 *
 * ==================== 代码框架总览 ====================
 *
 *  ┌─────────────────────────────────────────────────────────────┐
 *  │              PCI 驱动 + chrdev 字符设备 + MSI 中断            │
 *  │                                                             │
 *  │  1. pci_device_id       ──► 匹配 EDU 设备 (1234:11e8)        │
 *  │  2. module_init         ──► chrdev 号预分配 + class 创建     │
 *  │  3. probe / remove     ──► PCI 设备生命周期管理              │
 *  │  4. BAR0 映射          ──► pci_iomap() MMIO 访问           │
 *  │  5. MSI 向量分配        ──► pci_alloc_irq_vectors()         │
 *  │  6. request_irq        ──► 注册中断处理函数                  │
 *  │  7. chrdev             ──► cdev + device_create 用户接口    │
 *  │  8. file_operations    ──► open / ioctl                    │
 *  └─────────────────────────────────────────────────────────────┘
 *
 * ==================== 学习重点 ====================
 *
 *  day24 实现了驱动与用户态通过 BAR2 共享内存通信，但设备无法主动通知 CPU。
 *  day25 的核心问题是：QEMU 模拟的 EDU 设备，如何通过 MSI 向 CPU 发中断？
 *
 *  阶段分工：
 *    day22 = 设备发现 + PCI 骨架
 *    day23 = PCI 资源接管（BAR iomap）
 *    day24 = BAR2 共享内存协议 + miscdevice
 *    day25 = MSI 中断 ← 今天（chrdev + MSI + request_irq）
 *    day26+ = 深入学习
 *
 * ==================== EDU 设备背景 ====================
 *
 *  EDU = QEMU Virtualization Teaching Device
 *    Vendor ID: 1234
 *    Device ID: 11e8
 *    BAR0 大小: 4KB（寄存器空间）
 *    中断类型: MSI（Message Signaled Interrupt）
 *
 *  为什么用 EDU 而不是 ivshmem-doorbell：
 *    ivshmem 文档不清晰，需要研究 QEMU 源码
 *    EDU 是专门的教学设备，寄存器布局清晰明确
 *
 *  BAR0 寄存器（32-bit MMIO）：
 *    0x00  ID           只读   → 设备ID（验证驱动连接）
 *    0x04  LIVENESS     读写   → 写入 0xa5a5aa5a，设备返回按位取反值
 *    0x20  STATUS       只读   → 设备状态
 *    0x24  IRQ_STATUS   只读   → 当前等待中的中断源
 *    0x60  IRQ_RAISE    只写   → 写入任意值触发 MSI 中断
 *    0x64  IRQ_ACK      只写   → 写入 IRQ_STATUS 的值清除中断
 *
 *  LIVENESS 为什么用 0xa5a5aa5a：
 *    设备对写入值做按位取反（bitwise NOT）
 *    写入:  0xa5a5aa5a → 读出: 0x5a5a55a5
 *    如果不是这个值，说明 MMIO 映射有问题
 */

#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "include/day25_edu_irq.h"
#include "../include/day25_edu_uapi.h"

/*
 * ==================== 第1步：全局资源（module_init 时分配） ====================
 *
 * 整个模块共享一组字符设备主设备号和 class：
 *   - g_day25_base_dev：alloc_chrdev_region() 分配到的主设备号
 *   - g_day25_class：device_create() 依赖的 class，devtmpfs/sysfs 都会用到
 *   - g_day25_minor：当前已经分配了多少个 minor
 *
 * 为什么这些是全局变量而不是 day25_dev 的成员？
 *   class_create 只需要执行一次，所有设备共享
 *   chrdev 主设备号也只需要分配一次
 *   EDU 实验通常只有一个设备，但按通用方式实现便于扩展多设备
 *
 * 注意：这些在 module_init 中分配，module_exit 中释放
 */
static dev_t g_day25_base_dev;         /* 主设备号（内核分配） */
static struct class *g_day25_class;    /* sysfs class（/sys/class/day25_edu/） */
static atomic_t g_day25_minor = ATOMIC_INIT(0);  /* 已分配的次设备号计数器 */

/*
 * ==================== 第2步：EDU 寄存器读写封装 ====================
 *
 * EDU 寄存器都是 MMIO 32-bit 寄存器。
 * 封装成 read32/write32 的原因：
 *   1. 统一偏移语义（off 是 BAR0 内偏移）
 *   2. 便于加 trace/log 时不需要改很多调用点
 *   3. 与 day24 的 day24_proto_read32/write32 设计一致
 *
 * 为什么不直接用 readl/writel？
 *   readl/writel 需要传 MMIO 虚拟地址 + 偏移
 *   这里封装后只需传 day25_dev* + off，内部自己算地址
 */
static u32 day25_read32(struct day25_dev *d, u32 off)
{
    return readl(d->bar0 + off);  /* d->bar0 是 BAR0 映射后的虚拟地址 */
}

static void day25_write32(struct day25_dev *d, u32 off, u32 val)
{
    writel(val, d->bar0 + off);
}

/*
 * ==================== 第3步：中断处理函数（MSI handler） ====================
 *
 * MSI 中断的处理函数，运行在"中断上下文"（不是进程上下文）
 *
 * 中断上下文的特点：
 *   - 不能睡眠（不能调用 schedule、mutex_lock 等）
 *   - 不能访问用户态内存（没有 current->mm）
 *   - 必须使用自旋锁（spin_lock）而非 mutex
 *
 * handler 的工作流程：
 *   1. 读 IRQ_STATUS，确认中断源（0 = 无效中断，返回 IRQ_NONE）
 *   2. 自旋锁保护下更新 irq_count / last_irq_status / last_ack_value
 *   3. 向 IRQ_ACK 写同样的值，清除中断
 *   4. 打印日志（dmesg 中可见）
 *
 * 为什么先读 IRQ_STATUS 再判断？
 *   MSI 向量是预先分配好的，但同一个 MSI 向量可能被多个设备共享
 *   （虽然 day25 只有一个设备）
 *   读 IRQ_STATUS 确认"确实是我们设备的中断"
 */
static irqreturn_t day25_irq_handler(int irq, void *opaque)
{
    struct day25_dev *d = opaque;       /* 从 request_irq 的参数传来 */
    unsigned long flags;                 /* 保存中断状态标志 */
    u32 status;

    /*
     * ========== 第1步：读取 IRQ_STATUS ==========
     * status 的哪些位为1，表示哪些中断源在等待
     */
    status = day25_read32(d, DAY25_EDU_REG_IRQ_STATUS);
    if (!status)
        return IRQ_NONE;  /* 不是我们的中断，让系统继续查找其他 handler */

    /*
     * ========== 第2步：自旋锁保护共享数据 ==========
     * spin_lock_irqsave = 关中断 + 加锁
     * 为什么需要关中断？
     *   防止中断处理过程中被同一中断嵌套打断
     *   同一 MSI 中断不会嵌套，但关了中断更安全
     */
    spin_lock_irqsave(&d->irq_lock, flags);
    d->irq_count++;              /* 中断次数统计 */
    d->last_irq_status = status; /* 最近一次中断的 status */
    d->last_ack_value = status;  /* 最近一次 ACK 的值 */
    spin_unlock_irqrestore(&d->irq_lock, flags);

    /*
     * ========== 第3步：清除中断 ==========
     * 往 IRQ_ACK 写 status 的值，清除对应中断
     * 这告诉 EDU 硬件："这个中断我已经处理完了"
     */
    day25_write32(d, DAY25_EDU_REG_IRQ_ACK, status);

    /*
     * ========== 第4步：打印日志 ==========
     * 用户可以用 dmesg | grep "irq handler" 看到
     */
    dev_info(&d->pdev->dev,
             "irq handler: irq=%d status=0x%08x count=%llu",
             irq, status, d->irq_count);
    return IRQ_HANDLED;  /* 告诉内核：中断已处理 */
}

/*
 * ==================== 第4步：file_operations - open ====================
 *
 * open() 的核心任务：建立 file->private_data 关联
 *
 * 为什么 chrdev 的 open 需要 container_of？
 *   inode->i_cdev 指向 cdev 结构体
 *   cdev 是 day25_dev 的成员
 *   container_of 通过成员地址反推结构体基地址
 *
 * 与 day24 miscdevice 的区别：
 *   miscdevice: file->private_data = miscdevice*，直接拿到
 *   chrdev:     file->private_data = i_cdev，需要 container_of 反推
 */
static int day25_open(struct inode *inode, struct file *file)
{
    struct day25_dev *d = container_of(inode->i_cdev, struct day25_dev, cdev);
    // container_of：已知 i_cdev 在 day25_dev 中的偏移，反推 day25_dev 指针
    file->private_data = d;
    return 0;
}

/*
 * ==================== 第5步：file_operations - ioctl ====================
 *
 * ioctl 是 day25 用户态验证的主要控制入口：
 *   GET_INFO        → 读取 probe 阶段得到的静态信息 + 当前中断状态
 *   TRIGGER_IRQ     → 往 EDU 的 IRQ_RAISE 寄存器写值，触发一次 MSI 中断
 *   GET_IRQ_COUNT   → 获取驱动内累计中断次数
 *   GET_IRQ_STATUS  → 获取最近一次中断的 status 和 ack_value
 *
 * TRIGGER_IRQ 的原理：
 *   writel(trig.value, BAR0 + 0x60) → EDU 收到写 → 发 MSI 到 CPU
 *   CPU 执行 day25_irq_handler()
 *
 * 为什么 GET_IRQ_COUNT 需要在锁内读？
 *   irq_count 在 handler 中被修改（spin_lock 保护）
 *   用户态 ioctl 读的时候也可能正在修改
 *   但 u64 读取在多数架构是原子的，所以 day25 没加锁
 *   生产代码应该用 seqlock 或 atomic_t
 */
static long day25_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct day25_dev *d = file->private_data;

    switch (cmd) {

    /*
     * ========== GET_INFO ==========
     * 返回所有静态 + 动态信息给用户态
     * 包括：PCI 信息、BAR0 信息、MSI 向量、irq_count、liveness 验证值
     */
    case DAY25_IOC_GET_INFO: {
        struct day25_info info = {
            .vendor_id = d->pdev->vendor,
            .device_id = d->pdev->device,
            .bar0_start = d->bar0_start,
            .bar0_len = d->bar0_len,
            .irq_vector = d->irq_vector,
            .irq_count = d->irq_count,
            .last_irq_status = d->last_irq_status,
            .last_ack_value = d->last_ack_value,
            .liveness_value = d->liveness_value,
            .liveness_inverted = d->liveness_inverted,
            .msi_enabled = !!(d->pdev->msi_enabled),
        };
        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;
    }

    /*
     * ========== TRIGGER_IRQ ==========
     * 核心：往 IRQ_RAISE 写值，触发 MSI 中断
     *
     * 用户态传入 struct day25_trigger { u32 value; }
     * value 的值会出现在 handler 的 status 中
     * 通常写 1，但任意值都可以
     */
    case DAY25_IOC_TRIGGER_IRQ: {
        struct day25_trigger trig;
        if (copy_from_user(&trig, (void __user *)arg, sizeof(trig)))
            return -EFAULT;
        dev_info(&d->pdev->dev, "trigger irq: value=0x%08x", trig.value);
        day25_write32(d, DAY25_EDU_REG_IRQ_RAISE, trig.value);
        return 0;
    }

    /*
     * ========== GET_IRQ_COUNT ==========
     * 返回 irq_count = handler 被调用了多少次
     * trigger 前后各查一次，差值就是中断次数
     */
    case DAY25_IOC_GET_IRQ_COUNT: {
        struct day25_irq_count cnt = { .count = d->irq_count };
        if (copy_to_user((void __user *)arg, &cnt, sizeof(cnt)))
            return -EFAULT;
        return 0;
    }

    /*
     * ========== GET_IRQ_STATUS ==========
     * 返回最近一次中断的 status 和 ack_value
     */
    case DAY25_IOC_GET_IRQ_STATUS: {
        struct day25_irq_status st = {
            .irq_status = d->last_irq_status,
            .ack_value = d->last_ack_value,
        };
        if (copy_to_user((void __user *)arg, &st, sizeof(st)))
            return -EFAULT;
        return 0;
    }

    default:
        return -ENOTTY;  /* 未知命令 */
    }
}

/*
 * file_operations 定义完成
 * day25 没有实现 read/write，只用 ioctl 就足够验证中断机制
 */
static const struct file_operations day25_fops = {
    .owner = THIS_MODULE,
    .open = day25_open,
    .unlocked_ioctl = day25_ioctl,
    .llseek = no_llseek,  /* 不需要 llseek */
};

/*
 * ==================== 第6步：chrdev 设备节点创建 ====================
 *
 * probe 成功后为当前 EDU 设备建立字符设备节点。
 *
 * 为什么不在 module_init 中创建设备节点？
 *   module_init 在 insmod 时执行，probe 在 QEMU 启动后 PCI 枚举到设备才执行
 *   一个是"驱动加载"，一个是"设备连接"，时序不同
 *
 * 为什么需要 minor 计数？
 *   g_day25_minor 记录已分配的次设备号
 *   EDU 实验通常只有一个设备，但设计成可扩展
 *
 * sysfs 关系：
 *   /sys/class/day25_edu/day25_edu0/ ← device_create() 创建
 *   /sys/bus/pci/devices/.../        ← PCI 设备自己的目录
 */
static int day25_setup_chrdev(struct day25_dev *d)
{
    int ret;
    int minor = atomic_fetch_add(1, &g_day25_minor);  /* 分配一个次设备号 */

    /*
     * 构建设备号：主设备号 + 次设备号
     */
    d->devt = MKDEV(MAJOR(g_day25_base_dev), minor);

    /*
     * 初始化 cdev 并绑定 file_operations
     */
    cdev_init(&d->cdev, &day25_fops);
    d->cdev.owner = THIS_MODULE;

    /*
     * 将 cdev 添加到内核的字符设备表中
     * 之后 VFS 根据设备号就能找到这个 cdev
     */
    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret)
        return ret;

    /*
     * 创建设备节点：
     *   /dev/day25_edu0 （如果 devtmpfs 可用）
     *   /sys/class/day25_edu/day25_edu0/
     * parent = &d->pdev->dev 建立设备关系
     */
    d->device = device_create(g_day25_class, &d->pdev->dev, d->devt, d,
                              DAY25_DEV_NAME_FMT, minor);
    if (IS_ERR(d->device)) {
        ret = PTR_ERR(d->device);
        d->device = NULL;
        cdev_del(&d->cdev);
        return ret;
    }

    return 0;
}

/*
 * 销毁 chrdev（remove 中调用）
 * 注意顺序：先 device_destroy，再 cdev_del
 */
static void day25_destroy_chrdev(struct day25_dev *d)
{
    if (d->device)
        device_destroy(g_day25_class, d->devt);
    cdev_del(&d->cdev);
}

/*
 * ==================== 第7步：probe - 设备插入时调用 ====================
 *
 * 当内核 PCI core 枚举到 1234:11e8 时自动调用
 *
 * probe 内部分解（资源获取顺序）：
 *
 *  Step 1: kzalloc ──► 分配私有数据结构
 *           注意：不用 devm_kzalloc，因为后续资源需要手动管理
 *
 *  Step 2: spin_lock_init ──► 初始化自旋锁
 *           注意：必须在使用锁之前初始化
 *
 *  Step 3: pci_set_drvdata ──► 建立 pdev ↔ d 双向指针
 *
 *  Step 4: pci_enable_device ──► 使能 PCI 设备
 *           激活 BAR 地址解码
 *
 *  Step 5: pci_request_regions ──► 申请 BAR 资源独占访问
 *
 *  Step 6: pci_set_master ──► 设置为主设备模式（设备可发起 DMA）
 *
 *  Step 7: pci_iomap ──► 映射 BAR0 到虚拟地址
 *           注意：只映射 BAR0，不映射 BAR1/2/3/4/5
 *
 *  Step 8: pci_alloc_irq_vectors ──► 分配 MSI 向量
 *           day25 核心：申请 MSI，不降级到 INTx
 *
 *  Step 9: pci_irq_vector ──► 获取分配的 IRQ 号
 *
 *  Step 10: request_irq ──► 注册中断处理函数
 *
 *  Step 11: 读写 LIVENESS 寄存器 ──► 验证 MMIO 映射正确
 *
 *  Step 12: day25_setup_chrdev ──► 创建字符设备节点
 *
 * 错误处理（逆序）：
 *   err_irq       → free_irq
 *   err_iounmap   → pci_iounmap
 *   err_regions   → pci_release_regions
 *   err_disable   → pci_disable_device
 *   err_free      → kfree
 */
static int day25_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day25_dev *d;
    int ret;
    u32 ident;
    u32 live;

    dev_info(&pdev->dev, "probe enter: %04x:%04x", pdev->vendor, pdev->device);

    /*
     * ========== Step 1: 分配私有数据 ==========
     */
    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;
    d->pdev = pdev;
    spin_lock_init(&d->irq_lock);       /* 初始化自旋锁 */
    pci_set_drvdata(pdev, d);           /* 绑定双向指针 */

    /*
     * ========== Step 2: 使能 PCI 设备 ==========
     */
    ret = pci_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "pci_enable_device failed: %d", ret);
        goto err_free;
    }

    /*
     * ========== Step 3: 申请 BAR 资源 ==========
     */
    ret = pci_request_regions(pdev, DAY25_DRV_NAME);
    if (ret) {
        dev_err(&pdev->dev, "pci_request_regions failed: %d", ret);
        goto err_disable;
    }

    /*
     * ========== Step 4: 设置为主设备 ==========
     */
    pci_set_master(pdev);

    /*
     * ========== Step 5: 读取 BAR0 信息并映射 ==========
     */
    d->bar0_start = pci_resource_start(pdev, DAY25_BAR0);
    d->bar0_len = pci_resource_len(pdev, DAY25_BAR0);
    d->bar0 = pci_iomap(pdev, DAY25_BAR0, 0);  /* 映射 BAR0 */
    if (!d->bar0) {
        ret = -ENOMEM;
        dev_err(&pdev->dev, "pci_iomap BAR0 failed");
        goto err_regions;
    }

    /*
     * ========== Step 6: 分配 MSI 向量 ==========
     *
     * pci_alloc_irq_vectors 参数：
     *   参数1：最少分配 1 个向量
     *   参数2：最多分配 1 个向量
     *   参数3：只要 MSI（不要 INTx 兜底）
     *
     * 返回值：
     *   < 0 表示失败
     *   > 0 表示实际分配的向量数
     *
     * 为什么不允许降级到 INTx？
     *   day25 的目标就是学习 MSI，INTx 没意义
     */
    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
    if (ret < 0) {
        dev_err(&pdev->dev, "pci_alloc_irq_vectors(MSI) failed: %d", ret);
        goto err_iounmap;
    }

    /*
     * ========== Step 7: 获取 IRQ 号并注册 handler ==========
     */
    d->irq_vector = pci_irq_vector(pdev, 0);  /* 获取第 0 个向量对应的 IRQ 号 */
    ret = request_irq(d->irq_vector,          /* IRQ 号 */
                      day25_irq_handler,      /* 中断处理函数 */
                      0,                      /* flags（0 = 边沿触发） */
                      DAY25_DRV_NAME,        /* 中断名字（出现在 /proc/interrupts） */
                      d);                     /* 传给 handler 的参数 */
    if (ret) {
        dev_err(&pdev->dev, "request_irq failed: %d", ret);
        goto err_irq_vectors;
    }

    /*
     * ========== Step 8: 验证 MMIO 可访问（LIVENESS 测试） ==========
     *
     * LIVENESS 原理：
     *   写入: 0xa5a55a5a → EDU 返回按位取反: 0x5a5aa5a5
     *   如果不是这个值，说明 MMIO 映射有问题
     *
     * 这是 day25 验证 BAR0 iomap 成功的核心方式
     */
    ident = day25_read32(d, DAY25_EDU_REG_ID);     /* 应该返回 EDU ID */
    live = 0xa5a55a5a;
    day25_write32(d, DAY25_EDU_REG_LIVENESS, live);
    d->liveness_value = live;
    d->liveness_inverted = day25_read32(d, DAY25_EDU_REG_LIVENESS);

    /*
     * 打印所有关键信息到 dmesg
     */
    dev_info(&pdev->dev,
             "BAR0: start=0x%llx len=0x%llx flags=0x%llx",
             (unsigned long long)d->bar0_start,
             (unsigned long long)d->bar0_len,
             (unsigned long long)pci_resource_flags(pdev, DAY25_BAR0));
    dev_info(&pdev->dev,
             "MSI vector=%d ident=0x%08x liveness=0x%08x inverted=0x%08x",
             d->irq_vector, ident, d->liveness_value, d->liveness_inverted);

    /*
     * ========== Step 9: 建立字符设备节点 ==========
     * 成功后 /dev/day25_edu0 就创建了
     */
    ret = day25_setup_chrdev(d);
    if (ret) {
        dev_err(&pdev->dev, "setup chrdev failed: %d", ret);
        goto err_irq;
    }

    dev_info(&pdev->dev, "probe success");
    return 0;

/*
 * ==================== 错误处理（逆序释放） ====================
 */
err_irq:
    free_irq(d->irq_vector, d);  /* 注销中断处理函数 */
err_irq_vectors:
    pci_free_irq_vectors(pdev);   /* 释放 MSI 向量 */
err_iounmap:
    pci_iounmap(pdev, d->bar0);  /* 解除 BAR0 映射 */
err_regions:
    pci_release_regions(pdev);    /* 释放 BAR 资源 */
err_disable:
    pci_disable_device(pdev);     /* 关闭设备 */
err_free:
    pci_set_drvdata(pdev, NULL);
    kfree(d);                      /* 释放私有数据 */
    return ret;
}

/*
 * ==================== 第8步：remove - 设备拔出时调用 ====================
 *
 * remove 的核心理念：完全对称 probe
 *   probe 获取了什么资源，remove 就按逆序释放什么资源
 *
 * 为什么顺序重要？
 *   probe: enable → request_regions → iomap → irq_vectors → request_irq → chrdev
 *   remove: chrdev_destroy → free_irq → free_irq_vectors → iounmap → regions → disable
 *
 * 注意：
 *   device_destroy 和 cdev_del 在 free_irq 之前执行
 *   因为设备节点存在期间，中断处理函数可能还被调用
 */
static void day25_remove(struct pci_dev *pdev)
{
    struct day25_dev *d = pci_get_drvdata(pdev);

    if (!d)
        return;

    dev_info(&pdev->dev, "remove enter");

    /*
     * ========== Step 1: 销毁字符设备节点 ==========
     */
    day25_destroy_chrdev(d);

    /*
     * ========== Step 2: 注销中断处理函数 ==========
     * 注意：必须先注销 irq，再释放 MSI 向量
     */
    free_irq(d->irq_vector, d);

    /*
     * ========== Step 3: 释放 MSI 向量 ==========
     */
    pci_free_irq_vectors(pdev);

    /*
     * ========== Step 4: 解除 BAR0 映射 ==========
     */
    if (d->bar0)
        pci_iounmap(pdev, d->bar0);

    /*
     * ========== Step 5: 释放 BAR 资源 + 关闭设备 ==========
     */
    pci_release_regions(pdev);
    pci_disable_device(pdev);

    /*
     * ========== Step 6: 释放私有数据 ==========
     */
    pci_set_drvdata(pdev, NULL);
    kfree(d);

    dev_info(&pdev->dev, "remove leave");
}

/*
 * ==================== 第9步：pci_driver 结构体 ====================
 */
static const struct pci_device_id day25_ids[] = {
    { PCI_DEVICE(DAY25_EDU_VENDOR_ID, DAY25_EDU_DEVICE_ID) },
    //        = PCI_DEVICE(0x1234, 0x11e8)  ← QEMU EDU 教学设备
    { 0, }  /* 结束标记 */
};
MODULE_DEVICE_TABLE(pci, day25_ids);

static struct pci_driver day25_pci_driver = {
    .name = DAY25_DRV_NAME,
    .id_table = day25_ids,
    .probe = day25_probe,
    .remove = day25_remove,
};

/*
 * ==================== 第10步：module_init / module_exit ====================
 *
 * 为什么 day25 需要自定义 init/exit，而 day24 用 module_pci_driver？
 *
 * day24（miscdevice）：
 *   miscdevice 的注册不需要提前分配资源
 *   misc_register() 可以直接在 probe 中调用
 *   所以用 module_pci_driver 宏就够了
 *
 * day25（chrdev + class）：
 *   class_create 和 alloc_chrdev_region 必须在所有 probe 之前执行
 *   因为：
 *     1. module_init 在 insmod 时执行，早于 QEMU 启动
 *     2. QEMU 启动后 PCI 枚举到设备才调用 probe
 *     3. probe 中的 device_create 需要 g_day25_class 已经存在
 *   所以必须用 module_init 手动控制执行顺序
 *
 * 执行顺序：
 *   insmod → module_init → alloc_chrdev_region + class_create + pci_register_driver
 *                         （probe 不会立即调用，等 QEMU 启动）
 *
 *   QEMU 启动 → PCI 枚举 → probe 被调用
 *
 * rmmod → module_exit → pci_unregister_driver（所有 remove 被调用）+ class_destroy + unregister_chrdev_region
 */
static int __init day25_init(void)
{
    int ret;

    /*
     * ========== Step 1: 分配字符设备号范围 ==========
     * alloc_chrdev_region(&dev, firstminor, count, name)
     *   firstminor=0：次设备号从 0 开始
     *   count=DAY25_MAX_MINORS：最多 32 个次设备
     *   name="day25_edu"：出现在 /proc/devices
     */
    ret = alloc_chrdev_region(&g_day25_base_dev, 0, DAY25_MAX_MINORS, DAY25_DRV_NAME);
    if (ret)
        return ret;

    /*
     * ========== Step 2: 创建 sysfs class ==========
     * class_create 后会在 /sys/class/day25_edu/ 创建目录
     * 后续 device_create 会在这个目录下创建设备符号链接
     */
    g_day25_class = class_create(THIS_MODULE, DAY25_CLASS_NAME);
    if (IS_ERR(g_day25_class)) {
        ret = PTR_ERR(g_day25_class);
        g_day25_class = NULL;
        unregister_chrdev_region(g_day25_base_dev, DAY25_MAX_MINORS);
        return ret;
    }

    /*
     * ========== Step 3: 注册 PCI 驱动 ==========
     * pci_register_driver 注册后，内核会在 PCI 总线枚举时匹配设备
     * probe 不会在这里立即调用（设备可能还没枚举到）
     */
    ret = pci_register_driver(&day25_pci_driver);
    if (ret) {
        class_destroy(g_day25_class);
        g_day25_class = NULL;
        unregister_chrdev_region(g_day25_base_dev, DAY25_MAX_MINORS);
        return ret;
    }

    pr_info("day25_edu_irq: module loaded");
    return 0;
}

static void __exit day25_exit(void)
{
    /*
     * pci_unregister_driver 会自动调用所有已注册设备的 remove
     * 这是 module_exit 第一步必须做的
     */
    pci_unregister_driver(&day25_pci_driver);

    /*
     * 之后销毁 class 和释放设备号
     * 注意：device_destroy 会在 remove 中自动调用
     */
    if (g_day25_class)
        class_destroy(g_day25_class);
    unregister_chrdev_region(g_day25_base_dev, DAY25_MAX_MINORS);

    pr_info("day25_edu_irq: module unloaded");
}

module_init(day25_init);
module_exit(day25_exit);

/*
 * ==================== 模块信息 ====================
 */
MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Day25 EDU MSI interrupt experiment");
MODULE_LICENSE("GPL");

/*
 * ==================== 附录：完整数据流图 ====================
 *
 * 用户态进程                     驱动                         EDU 硬件
 * ─────────────────────────────────────────────────────────────────────
 * insmod
 *   → day25_init()
 *     → alloc_chrdev_region()
 *     → class_create()
 *     → pci_register_driver()
 *   → 等待 QEMU 启动
 *
 * [QEMU 启动，创建 EDU 设备 1234:11e8]
 *   → PCI 总线枚举
 *   → 匹配到 day25_pci_driver.id_table
 *   → day25_probe() 被调用
 *     → pci_enable_device()
 *     → pci_request_regions()
 *     → pci_iomap(BAR0) → bar0 虚拟地址
 *     → pci_alloc_irq_vectors(MSI)
 *     → request_irq(handler)
 *     → 读写 LIVENESS 验证 MMIO
 *     → day25_setup_chrdev()
 *       → cdev_add()
 *       → device_create()
 *         → /dev/day25_edu0 创建！
 *
 * open("/dev/day25_edu0")
 *   → day25_open()
 *     → container_of(inode->i_cdev, ...) 反推 day25_dev*
 *     → file->private_data = d
 *
 * ioctl(TRIGGER_IRQ, value=1)
 *   → day25_ioctl(TRIGGER_IRQ)
 *     → writel(1, BAR0 + 0x60)  ← IRQ_RAISE
 *       ──────────────────────────► EDU 收到写
 *                                    ↓
 *                              MSI 写事务发到 PCI 总线
 *                                    ↓
 *                              CPU 收到 MSI 中断
 *                                    ↓
 *                          day25_irq_handler() 被调用
 *                                    ↓
 *                          status = readl(BAR0 + 0x24)  // IRQ_STATUS
 *                          if status != 0:
 *                            irq_count++
 *                            writel(status, BAR0 + 0x64)  // IRQ_ACK
 *                          return IRQ_HANDLED
 *
 * ioctl(GET_IRQ_COUNT)
 *   → day25_ioctl(GET_IRQ_COUNT)
 *     → return irq_count
 *
 * rmmod
 *   → day25_exit()
 *     → pci_unregister_driver()
 *       → 所有设备的 day25_remove() 被调用
 *         → day25_destroy_chrdev()
 *         → free_irq()
 *         → pci_free_irq_vectors()
 *         → pci_iounmap()
 *         → pci_release_regions()
 *         → pci_disable_device()
 *         → kfree()
 *     → class_destroy()
 *     → unregister_chrdev_region()
 */
