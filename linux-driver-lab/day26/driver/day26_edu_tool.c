// SPDX-License-Identifier: GPL-2.0
/*
 * day26_edu_tool.c - EDU 用户态友好工具驱动
 *
 * ==================== 文件概述 ====================
 *
 * Day26 在 Day25 EDU + MSI 中断实验基础上，将字符设备接口打磨成用户态友好工具。
 *
 * 【学习焦点】
 *   1. 用户态友好设计：read() 返回文本、write() 接受自然输入、ioctl 返回结构化数据
 *   2. 清晰错误码：EINVAL/E2BIG/ENODEV/EFAULT，每种错误都有明确来源
 *   3. 用户态工具：完整 CLI 工具，与驱动配合完成正向+负向测试
 *   4. 错误验证：故意要求触发值非零，验证负向路径是否正确
 *
 * 【与 Day25 的区别】
 *   Day25：只有 ioctl 接口（GET_INFO、TRIGGER_IRQ、GET_IRQ_COUNT、GET_IRQ_STATUS）
 *   Day26：在 Day25 基础上增加 read()（文本状态）和 write()（整数触发）
 *
 * 【硬件模型】
 *   底层硬件与 Day25 完全相同：QEMU 模拟的 EDU 教学设备 (1234:11e8)
 *   - BAR0 MMIO 映射到寄存器
 *   - MSI 中断机制（写入 IRQ_RAISE 触发中断）
 *   - LIVENESS 验证（写入 0xa5a55a5a 读回取反值）
 *
 * ==================== 代码结构 ====================
 *
 *  1. 全局资源（模块级别）
 *  2. EDU 寄存器读写封装
 *  3. MSI 中断处理函数
 *  4. 文本状态快照生成（read 用）
 *  5. file_operations（open/read/write/ioctl）
 *  6. 字符设备注册/销毁
 *  7. PCI probe/remove（设备枚举时调用）
 *  8. 模块 init/exit
 *
 * 【为什么这样组织？】
 *   → PCI 驱动是平台相关代码，必须遵循 pci_driver 结构
 *   → chrdev 是字符设备抽象，绑定 file_operations
 *   → 中断处理必须在独立函数（request_irq 要求）
 *   → 模块 init/exit 负责分配/释放全局资源
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

#include "include/day26_edu_tool.h"
#include "../include/day26_edu_uapi.h"

/*
 * ==================== 第1部分：全局字符设备资源 ====================
 *
 * 这些是模块级别的全局变量，管理整个驱动的资源。
 *
 * 为什么需要全局资源？
 *   → alloc_chrdev_region 分配的是设备号（dev_t），整个模块共享一个 major
 *   → class_create 创建 sysfs 类，整个模块共享一个类
 *   → g_day26_minor 原子变量支持将来扩展多设备（每个设备分配不同 minor）
 *
 * 【设计思路】
 *   - 一个 major + 多个 minor：支持未来挂载多个 EDU 设备
 *   - 当前实验只用到 minor 0，但基础设施已就绪
 *
 * 【对应关系】
 *   /sys/class/day26_edu/         ← g_day26_class
 *   /dev/day26_edu0, day26_edu1..  ← 每个设备节点
 */
static dev_t g_day26_base_dev;                 /* alloc_chrdev_region 分配的主设备号+次设备号范围 */
static struct class *g_day26_class;            /* sysfs 类，用于自动创建设备节点 */
static atomic_t g_day26_minor = ATOMIC_INIT(0); /* 原子次设备号分配器（线程安全） */

/*
 * ==================== 第2部分：EDU 寄存器读写封装 ====================
 *
 * 直接对 BAR0 MMIO 地址进行读写操作。
 *
 * 【MMIO 访问模式】
 *   BAR0 映射后的虚拟地址 → bar0
 *   读取：readl(bar0 + 偏移)
 *   写入：writel(值, bar0 + 偏移)
 *
 * 【为什么封装成函数而不是直接使用？】
 *   1. 便于调试：可以在函数内加日志
 *   2. 便于修改：如果将来改用 ioread32/iowrite32，修改一处即可
 *   3. 代码可读性：day26_read32(d, REG) 比 readl(d->bar0 + REG) 更清晰
 *
 * 【注意】
 *   readl/writel 是 Linux 内核提供的 MMIO 访问 API
 *   它们确保内存屏障和正确的数据宽度（32-bit）
 */
static u32 day26_read32(struct day26_dev *d, u32 off)
{
    /* 读取 BAR0 上偏移为 off 的 32-bit 寄存器 */
    return readl(d->bar0 + off);
}

static void day26_write32(struct day26_dev *d, u32 off, u32 val)
{
    /* 向 BAR0 上偏移为 off 的 32-bit 寄存器写入 val */
    writel(val, d->bar0 + off);
}

/*
 * ==================== 第3部分：MSI 中断处理函数 ====================
 *
 * 【中断处理流程】
 *   1. 用户 write() 触发 → 驱动写 IRQ_RAISE
 *   2. EDU 硬件发送 MSI 中断
 *   3. CPU 接收中断，调用此 handler
 *   4. 读取 IRQ_STATUS 确认中断源
 *   5. 更新计数器和状态
 *   6. 写 IRQ_ACK 清除中断
 *
 * 【irqreturn_t 返回值】
 *   - IRQ_NONE：表示这个中断不是我们的设备产生的
 *   - IRQ_HANDLED：表示我们已经处理了这个中断
 *
 * 【为什么先检查 status？】
 *   → 可能是共享中断（多个设备共用一个 IRQ 线）
 *   → 如果 status == 0，说明不是 EDU 触发的中断，快速返回
 *
 * 【为什么用 spin_lock_irqsave？】
 *   → 中断处理函数在中断上下文执行，不能睡眠
 *   → spin_lock 在中断上下文会自动禁用本地中断
 *   → irqsave 版本保存之前的 flags，解锁时恢复
 *
 * 【为什么要写 IRQ_ACK？】
 *   → EDU 的中断是电平触发的（或者需要软件确认）
 *   → 不写 ACK 会导致中断一直挂起，CPU 会再次触发
 */
static irqreturn_t day26_irq_handler(int irq, void *opaque)
{
    struct day26_dev *d = opaque;  /* 从 request_irq 传入的 opaque 参数获取设备结构 */
    unsigned long flags;
    u32 status;

    /* 第1步：读取中断状态寄存器 */
    status = day26_read32(d, DAY26_EDU_REG_IRQ_STATUS);

    /* 第2步：检查是否是我们的中断 */
    if (!status)
        return IRQ_NONE;  /* 不是我们的中断，快速返回 */

    /* 第3步：更新共享数据（需要加锁保护） */
    spin_lock_irqsave(&d->irq_lock, flags);  /* 保存中断状态并加锁 */
    d->irq_count++;                           /* 中断计数 +1 */
    d->last_irq_status = status;              /* 保存最近中断状态 */
    d->last_ack_value = status;               /* 保存 ACK 值（与 status 相同） */
    spin_unlock_irqrestore(&d->irq_lock, flags); /* 解锁并恢复中断状态 */

    /* 第4步：清除中断（向 IRQ_ACK 写入 status 值） */
    day26_write32(d, DAY26_EDU_REG_IRQ_ACK, status);

    /* 第5步：打印日志便于调试 */
    dev_info(&d->pdev->dev,
             "irq handler: irq=%d status=0x%08x count=%llu\n",
             irq, status, d->irq_count);

    return IRQ_HANDLED;  /* 告诉内核我们已经处理了这个中断 */
}

/*
 * ==================== 第4部分：生成文本状态快照 ====================
 *
 * 【设计目的】
 *   用户可以直接 read() 设备节点，获得一段可读的文本状态。
 *   不需要编写程序解析二进制结构体，直接 cat 即可查看。
 *
 * 【为什么不用 ioctl 返回结构体？】
 *   → 结构体需要定义、拷贝、解析，不够直观
 *   → 文本快照可以直接复制到日志、邮件、文档中
 *   → 适合人工阅读和调试
 *
 * 【scnprintf vs snprintf】
 *   scnprintf 返回实际写入的字节数（不包括 '\0'）
 *   snprintf 返回原本应该写入的字节数（如果缓冲区够用的话）
 *   → 使用 scnprintf 更安全，避免返回超过缓冲区大小的值
 *
 * 【输出格式示例】
 *   vendor=0x1234 device=0x11e8
 *   bar0_start=0x... bar0_len=0x...
 *   irq_vector=50 irq_count=1 msi_enabled=1
 *   identity_value=0x...
 *   liveness_value=0xa5a55a5a liveness_inverted=0x5a5aa5a5
 *   last_irq_status=0x... last_ack_value=0x...
 */
static ssize_t day26_build_state_text(struct day26_dev *d, char *buf, size_t size)
{
    return scnprintf(buf, size,
                     "vendor=0x%04x device=0x%04x\n"
                     "bar0_start=0x%llx bar0_len=0x%llx\n"
                     "irq_vector=%u irq_count=%llu msi_enabled=%u\n"
                     "identity_value=0x%08x\n"
                     "liveness_value=0x%08x liveness_inverted=0x%08x\n"
                     "last_irq_status=0x%08x last_ack_value=0x%08x\n",
                     d->pdev->vendor,
                     d->pdev->device,
                     (unsigned long long)d->bar0_start,
                     (unsigned long long)d->bar0_len,
                     d->irq_vector,
                     d->irq_count,
                     !!(d->pdev->msi_enabled),  /* 转换为 0/1 而非 bool 字符串 */
                     d->identity_value,
                     d->liveness_value,
                     d->liveness_inverted,
                     d->last_irq_status,
                     d->last_ack_value);
}

/*
 * ==================== 第5部分：file_operations - open ====================
 *
 * 【open() 的作用】
 *   用户打开设备节点时调用，用于初始化文件描述符。
 *
 * 【container_of 详解】
 *   内核链表容器宏，通过成员指针反推结构体指针。
 *   原理：已知 i_cdev 的地址，减去它在 day26_dev 中的偏移量。
 *
 *   inode->i_cdev 指向 day26_dev 结构体中的 cdev 成员
 *   通过 container_of 可以得到 day26_dev 的起始地址
 *
 * 【为什么需要 container_of？】
 *   → file->private_data 只能存一个 void*，我们存的是 day26_dev*
 *   → VFS 只知道 cdev，不知道 day26_dev
 *   → container_of 是连接两者的桥梁
 *
 * 【私有数据用途】
 *   将 day26_dev* 存入 file->private_data，后续 read/write/ioctl 可以直接使用
 */
static int day26_open(struct inode *inode, struct file *file)
{
    struct day26_dev *d = container_of(inode->i_cdev, struct day26_dev, cdev);
    file->private_data = d;
    return 0;  /* 返回 0 表示成功 */
}

/*
 * ==================== 第6部分：file_operations - read ====================
 *
 * 【read() 的作用】
 *   用户读取设备状态，返回文本格式的状态快照。
 *
 * 【设计思路】
 *   → 与 Day25 不同，Day26 的 read() 直接返回人类可读的文本
 *   → 用户不需要解析二进制结构体，直接 cat /dev/day26_edu0 即可
 *
 * 【simple_read_from_buffer】
 *   内核提供的辅助函数，简化从内核缓冲区复制到用户缓冲区的操作。
 *   自动处理 ppos（文件位置）和缓冲区边界。
 *
 * 【为什么用 kbuf 中转？】
 *   → 内核直接访问用户态指针危险（用户进程可能崩溃）
 *   → 先在内核栈分配缓冲区，构造好文本，再一次性拷贝到用户态
 *   → 更安全、更稳定
 */
static ssize_t day26_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct day26_dev *d = file->private_data;
    char kbuf[256];  /* 内核缓冲区，存放格式化文本 */
    ssize_t len;

    /* 检查设备是否正常初始化 */
    if (!d || !d->bar0)
        return -ENODEV;  /* 设备不存在或 BAR0 未映射 */

    /* 生成文本状态 */
    len = day26_build_state_text(d, kbuf, sizeof(kbuf));

    /* 复制到用户态缓冲区 */
    return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

/*
 * ==================== 第7部分：file_operations - write ====================
 *
 * 【write() 的作用】
 *   用户写入触发值，驱动将值写入 EDU 的 IRQ_RAISE 寄存器，触发 MSI 中断。
 *
 * 【输入格式】
 *   支持多种格式："1"、"0x1"、"0x5\n"
 *   使用 simple_strtoul 自动解析十进制和十六进制
 *
 * 【负向测试设计：触发值必须非零】
 *   这是 Day26 的刻意设计：
 *   → 如果 v == 0，返回 -EINVAL
 *   → 用于验证错误处理路径是否正确
 *   → 用户态工具测试 "trigger 0" 期望返回错误
 *
 * 【为什么需要这么复杂的验证？】
 *   → write() 是用户态向内核传输数据的主要通道
 *   → 必须验证数据有效性，防止恶意或错误输入导致问题
 *   → 每个错误码都有明确含义，便于调试
 *
 * 【错误码说明】
 *   -EINVAL：无效参数（count==0、空字符串、v==0、解析失败）
 *   -E2BIG：输入过长（超过 kbuf 能容纳的长度）
 *   -ENODEV：设备不可用（bar0 未映射）
 *   -EFAULT：用户数据拷贝失败（copy_from_user 返回非零）
 */
static ssize_t day26_write(struct file *file, const char __user *buf,
                           size_t count, loff_t *ppos)
{
    struct day26_dev *d = file->private_data;
    char kbuf[32];    /* 内核缓冲区，存放用户输入 */
    char *end;        /* strtoul 解析结束位置 */
    unsigned long v;  /* 解析后的触发值 */

    /* 检查设备是否正常 */
    if (!d || !d->bar0)
        return -ENODEV;

    /* 边界检查：count==0 表示没有数据 */
    if (count == 0)
        return -EINVAL;

    /* 输入过长：防止缓冲区溢出 */
    if (count >= sizeof(kbuf))
        return -E2BIG;

    /* 从用户态拷贝数据到内核态 */
    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;

    /* 添加字符串结束符，便于字符串函数处理 */
    kbuf[count] = '\0';

    /* 去除尾部空白字符（\n、\r、空格等） */
    strim(kbuf);

    /* 空字符串检查 */
    if (!kbuf[0])
        return -EINVAL;

    /* 解析整数：支持十进制(0x前缀=十六进制)、十六进制(0x前缀) */
    v = simple_strtoul(kbuf, &end, 0);

    /* 解析失败：end == kbuf 表示没有解析任何字符 */
    if (end == kbuf || *end != '\0')
        return -EINVAL;

    /* 【关键】触发值必须非零！这是故意设计的负向测试 */
    if (v == 0 || v > 0xffffffffUL)
        return -EINVAL;

    /* 打印日志 */
    dev_info(&d->pdev->dev, "write trigger: value=0x%08lx\n", v);

    /* 写入 EDU 的 IRQ_RAISE 寄存器，触发 MSI 中断 */
    day26_write32(d, DAY26_EDU_REG_IRQ_RAISE, (u32)v);

    /* 更新文件位置 */
    *ppos += count;

    return count;  /* 返回成功写入的字节数 */
}

/*
 * ==================== 第8部分：file_operations - ioctl ====================
 *
 * 【ioctl 的作用】
 *   提供结构化的设备控制接口，比 read/write 更精确地控制设备。
 *
 * 【Day26 支持的命令】
 *   - DAY26_IOC_GET_INFO：获取完整设备信息
 *   - DAY26_IOC_GET_IRQ_COUNT：获取中断计数
 *   - DAY26_IOC_GET_IRQ_STATUS：获取最近中断状态
 *   - DAY26_IOC_RESET_STATS：重置统计计数
 *
 * 【为什么要用结构体而不是直接返回数值？】
 *   → 可扩展性：将来可以在结构体中增加字段，不影响 ABI
 *   → 类型安全：避免 void* 带来的类型错误
 *   → 清晰性：每个命令返回什么数据一目了然
 *
 * 【copy_to_user 失败处理】
 *   如果拷贝失败（用户缓冲区可能已损坏），返回 -EFAULT
 *   不要返回部分数据，那样会让用户态解析出错误的结果
 */
static long day26_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct day26_dev *d = file->private_data;

    switch (cmd) {

    /* 【GET_INFO】返回完整设备信息 */
    case DAY26_IOC_GET_INFO: {
        struct day26_info info = {
            .tool_api_version = DAY26_TOOL_API_VERSION,  /* API 版本，便于工具兼容性检查 */
            .vendor_id = d->pdev->vendor,                /* PCI Vendor ID */
            .device_id = d->pdev->device,                /* PCI Device ID */
            .bar0_start = d->bar0_start,                 /* BAR0 起始物理地址 */
            .bar0_len = d->bar0_len,                     /* BAR0 长度 */
            .irq_vector = d->irq_vector,                 /* MSI 中断向量号 */
            .irq_count = d->irq_count,                   /* 中断处理计数 */
            .last_irq_status = d->last_irq_status,       /* 最近中断状态 */
            .last_ack_value = d->last_ack_value,         /* 最近 ACK 值 */
            .identity_value = d->identity_value,         /* ID 寄存器值 */
            .liveness_value = d->liveness_value,        /* LIVENESS 测试值 */
            .liveness_inverted = d->liveness_inverted,  /* LIVENESS 取反值 */
            .msi_enabled = !!(d->pdev->msi_enabled),     /* MSI 是否启用 */
        };
        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;
    }

    /* 【GET_IRQ_COUNT】返回中断计数 */
    case DAY26_IOC_GET_IRQ_COUNT: {
        struct day26_irq_count cnt = { .count = d->irq_count };
        if (copy_to_user((void __user *)arg, &cnt, sizeof(cnt)))
            return -EFAULT;
        return 0;
    }

    /* 【GET_IRQ_STATUS】返回最近中断状态 */
    case DAY26_IOC_GET_IRQ_STATUS: {
        struct day26_irq_status st = {
            .irq_status = d->last_irq_status,
            .ack_value = d->last_ack_value,
        };
        if (copy_to_user((void __user *)arg, &st, sizeof(st)))
            return -EFAULT;
        return 0;
    }

    /* 【RESET_STATS】重置统计计数 */
    case DAY26_IOC_RESET_STATS:
        d->irq_count = 0;
        d->last_irq_status = 0;
        d->last_ack_value = 0;
        return 0;

    default:
        /* 未知命令：返回 -ENOTTY（不适当的 ioctl） */
        return -ENOTTY;
    }
}

/*
 * 【file_operations 结构体定义】
 *
 * .owner = THIS_MODULE：防止模块卸载时文件操作还在被调用
 * .open：打开设备
 * .read：读取状态（文本快照）
 * .write：写入触发值（触发中断）
 * .unlocked_ioctl：设备控制（结构化命令）
 * .llseek：文件位置调整（使用默认实现）
 */
static const struct file_operations day26_fops = {
    .owner = THIS_MODULE,
    .open = day26_open,
    .read = day26_read,
    .write = day26_write,
    .unlocked_ioctl = day26_ioctl,
    .llseek = default_llseek,
};

/*
 * ==================== 第9部分：字符设备注册/销毁 ====================
 *
 * 【chrdev setup 流程】
 *   1. 分配 minor 号（atomic_fetch_add 保证线程安全）
 *   2. 构建设备号（MKDEV）
 *   3. 初始化 cdev（cdev_init）
 *   4. 添加到 VFS（cdev_add）
 *   5. 创建设备节点（device_create）
 *
 * 【chrdev destroy 流程（反向）】
 *   1. 销毁设备节点（device_destroy）
 *   2. 从 VFS 删除 cdev（cdev_del）
 *
 * 【device_create 参数】
 *   - g_day26_class：sysfs 类
 *   - &d->pdev->dev：父设备（PCI 设备会自动创建链接）
 *   - d->devt：设备号
 *   - d：传递给驱动的私有数据（可通过 sysfs 读取）
 *   - DAY26_DEV_NAME_FMT：设备名格式（"%s%d"）
 *   - minor：次设备号
 */
static int day26_setup_chrdev(struct day26_dev *d)
{
    int ret;
    int minor = atomic_fetch_add(1, &g_day26_minor);  /* 原子分配 minor 号 */

    /* 构建设备号：major + minor */
    d->devt = MKDEV(MAJOR(g_day26_base_dev), minor);

    /* 初始化 cdev，绑定 file_operations */
    cdev_init(&d->cdev, &day26_fops);
    d->cdev.owner = THIS_MODULE;

    /* 添加到 VFS，使设备节点可访问 */
    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret)
        return ret;  /* cdev_add 失败要返回错误码 */

    /* 创建 /dev/day26_eduX 设备节点 */
    d->device = device_create(g_day26_class, &d->pdev->dev, d->devt, d,
                              DAY26_DEV_NAME_FMT, minor);
    if (IS_ERR(d->device)) {
        /* device_create 失败要清理已添加的 cdev */
        ret = PTR_ERR(d->device);
        d->device = NULL;  /* 避免 remove 时 double free */
        cdev_del(&d->cdev);
        return ret;
    }
    return 0;
}

/* 字符设备销毁（remove 中调用） */
static void day26_destroy_chrdev(struct day26_dev *d)
{
    if (d->device)
        device_destroy(g_day26_class, d->devt);
    cdev_del(&d->cdev);
}

/*
 * ==================== 第10部分：PCI probe/remove ====================
 *
 * 【probe() 何时被调用？】
 *   当 PCI 总线枚举时，发现设备的 Vendor:Device ID 匹配 pci_device_id 数组中的条目。
 *   即：insmod 后，PCI 总线驱动遍历设备，找到 1234:11e8 时调用。
 *
 * 【probe() 职责】
 *   1. 分配私有数据结构
 *   2. 启用 PCI 设备
 *   3. 请求 BAR 资源
 *   4. 设置为主设备
 *   5. 映射 BAR0 MMIO
 *   6. 分配 MSI 中断向量
 *   7. 注册中断处理函数
 *   8. 读取硬件标识（LIVENESS）
 *   9. 注册字符设备
 *
 * 【remove() 何时被调用？】
 *   当模块卸载（rmmod）时，或设备从总线移除时。
 *   注意：PCI 总线热插拔可能触发 remove，但本实验环境无热插拔。
 *
 * 【错误处理模式】
 *   每个错误标签跳转到对应的清理代码，顺序与分配顺序相反。
 *   这是内核驱动的标准错误处理模式：分配顺序从小到大，释放顺序从大到小。
 */
static int day26_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day26_dev *d;
    int ret;

    dev_info(&pdev->dev, "probe enter: %04x:%04x\n", pdev->vendor, pdev->device);

    /* 第1步：分配私有数据结构 */
    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;
    d->pdev = pdev;

    /* 初始化自旋锁（保护中断相关共享数据） */
    spin_lock_init(&d->irq_lock);

    /* 将私有数据关联到 PCI 设备（remove 时可获取） */
    pci_set_drvdata(pdev, d);

    /* 第2步：启用 PCI 设备 */
    ret = pci_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "pci_enable_device failed: %d\n", ret);
        goto err_free;
    }

    /* 第3步：请求 BAR 资源（BAR0~BAR5） */
    ret = pci_request_regions(pdev, DAY26_DRV_NAME);
    if (ret) {
        dev_err(&pdev->dev, "pci_request_regions failed: %d\n", ret);
        goto err_disable;
    }

    /* 第4步：设置为主设备（启用总线主访问） */
    pci_set_master(pdev);

    /* 第5步：获取 BAR0 信息并映射 MMIO */
    d->bar0_start = pci_resource_start(pdev, DAY26_BAR0);
    d->bar0_len = pci_resource_len(pdev, DAY26_BAR0);
    d->bar0 = pci_iomap(pdev, DAY26_BAR0, 0);
    if (!d->bar0) {
        ret = -ENOMEM;
        dev_err(&pdev->dev, "pci_iomap BAR0 failed\n");
        goto err_regions;
    }

    /* 第6步：分配 MSI 中断向量（只分配 1 个） */
    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
    if (ret < 0) {
        dev_err(&pdev->dev, "pci_alloc_irq_vectors(MSI) failed: %d\n", ret);
        goto err_iounmap;
    }

    /* 第7步：获取中断向量号并注册中断处理函数 */
    d->irq_vector = pci_irq_vector(pdev, 0);
    ret = request_irq(d->irq_vector, day26_irq_handler, 0, DAY26_DRV_NAME, d);
    if (ret) {
        dev_err(&pdev->dev, "request_irq failed: %d\n", ret);
        goto err_free_vectors;
    }

    /* 第8步：读取硬件标识（probe 时只读探测，便于后续使用） */
    d->identity_value = day26_read32(d, DAY26_EDU_REG_ID);
    d->liveness_value = day26_read32(d, DAY26_EDU_REG_LIVENESS);
    d->liveness_inverted = ~d->liveness_value;

    /* 打印调试信息 */
    dev_info(&pdev->dev, "BAR0: start=0x%llx len=0x%llx\n",
             (unsigned long long)d->bar0_start,
             (unsigned long long)d->bar0_len);
    dev_info(&pdev->dev, "identity=0x%08x liveness=0x%08x inverted=0x%08x\n",
             d->identity_value, d->liveness_value, d->liveness_inverted);
    dev_info(&pdev->dev, "MSI vector=%d\n", d->irq_vector);

    /* 第9步：注册字符设备 */
    ret = day26_setup_chrdev(d);
    if (ret) {
        dev_err(&pdev->dev, "setup chrdev failed: %d\n", ret);
        goto err_irq;
    }

    dev_info(&pdev->dev, "probe success\n");
    return 0;

/* 错误处理标签（按分配倒序释放） */
err_irq:
    free_irq(d->irq_vector, d);          /* 释放中断 */
err_free_vectors:
    pci_free_irq_vectors(pdev);          /* 释放 MSI 向量 */
err_iounmap:
    pci_iounmap(pdev, d->bar0);         /* 解除 BAR0 映射 */
err_regions:
    pci_release_regions(pdev);           /* 释放 BAR 资源 */
err_disable:
    pci_disable_device(pdev);            /* 禁用 PCI 设备 */
err_free:
    pci_set_drvdata(pdev, NULL);
    kfree(d);                            /* 释放私有数据 */
    return ret;
}

/* PCI remove（模块卸载时调用） */
static void day26_remove(struct pci_dev *pdev)
{
    struct day26_dev *d = pci_get_drvdata(pdev);

    if (!d)
        return;

    dev_info(&pdev->dev, "remove enter\n");

    /* 销毁字符设备 */
    day26_destroy_chrdev(d);

    /* 释放中断 */
    free_irq(d->irq_vector, d);

    /* 释放 MSI 向量 */
    pci_free_irq_vectors(pdev);

    /* 解除 MMIO 映射 */
    if (d->bar0)
        pci_iounmap(pdev, d->bar0);

    /* 释放 BAR 资源 */
    pci_release_regions(pdev);

    /* 禁用 PCI 设备 */
    pci_disable_device(pdev);

    /* 清除私有数据指针 */
    pci_set_drvdata(pdev, NULL);

    /* 释放私有数据内存 */
    kfree(d);
}

/*
 * ==================== 第11部分：pci_driver 定义 ====================
 *
 * 【pci_driver 结构体】
 *   内核 PCI 子系统使用此结构体匹配设备并调用 probe/remove。
 *
 * 【pci_device_id 数组】
 *   定义支持的设备列表。PCI 总线枚举时，遍历所有设备的 vendor:device。
 *   匹配成功则调用驱动的 probe()。
 *
 * 【MODULE_DEVICE_TABLE】
 *   将 pci_device_id 导出到模块 metadata，加载时自动匹配设备。
 */
static const struct pci_device_id day26_ids[] = {
    { PCI_DEVICE(DAY26_EDU_VENDOR_ID, DAY26_EDU_DEVICE_ID) },  /* 1234:11e8 */
    { }
};
MODULE_DEVICE_TABLE(pci, day26_ids);

static struct pci_driver day26_pci_driver = {
    .name = DAY26_DRV_NAME,              /* 驱动名（出现在 /sys/bus/pci/drivers/ 下） */
    .id_table = day26_ids,                /* 支持的设备列表 */
    .probe = day26_probe,                 /* 设备匹配时调用 */
    .remove = day26_remove,               /* 模块卸载时调用 */
};

/*
 * ==================== 第12部分：模块 init/exit ====================
 *
 * 【init 流程】
 *   1. alloc_chrdev_region：分配主设备号和次设备号范围
 *   2. class_create：创建 sysfs 类
 *   3. pci_register_driver：注册 PCI 驱动（会触发 probe）
 *
 * 【exit 流程（反向）】
 *   1. pci_unregister_driver：注销 PCI 驱动
 *   2. class_destroy：销毁 sysfs 类
 *   3. unregister_chrdev_region：释放设备号
 *
 * 【为什么不用 module_pci_driver？】
 *   因为我们需要在 init 中做字符设备相关的 setup（alloc_chrdev_region、class_create）。
 *   module_pci_driver 只处理 pci_driver 的注册/注销，不够。
 *
 * 【pr_info vs dev_info】
 *   - pr_info：模块级别日志，打印到 kernel message buffer
 *   - dev_info：设备级别日志，可关联到特定 PCI 设备
 */
static int __init day26_init(void)
{
    int ret;

    /* 第1步：分配字符设备号（major:minor~minor+31） */
    ret = alloc_chrdev_region(&g_day26_base_dev, 0, 32, DAY26_DRV_NAME);
    if (ret)
        return ret;  /* 失败直接返回 */

    /* 第2步：创建 sysfs 类（自动创建设备节点） */
    g_day26_class = class_create(THIS_MODULE, DAY26_CLASS_NAME);
    if (IS_ERR(g_day26_class)) {
        unregister_chrdev_region(g_day26_base_dev, 32);  /* 回滚第1步 */
        return PTR_ERR(g_day26_class);  /* 返回错误码 */
    }

    /* 第3步：注册 PCI 驱动（会触发与设备的匹配，可能立即调用 probe） */
    ret = pci_register_driver(&day26_pci_driver);
    if (ret) {
        class_destroy(g_day26_class);                    /* 回滚第2步 */
        unregister_chrdev_region(g_day26_base_dev, 32); /* 回滚第1步 */
        return ret;
    }

    pr_info(DAY26_DRV_NAME ": module init\n");
    return 0;
}

static void __exit day26_exit(void)
{
    /* 注销顺序：先注销 PCI 驱动（防止新设备匹配） */
    pci_unregister_driver(&day26_pci_driver);

    /* 销毁 sysfs 类 */
    class_destroy(g_day26_class);

    /* 释放设备号 */
    unregister_chrdev_region(g_day26_base_dev, 32);

    pr_info(DAY26_DRV_NAME ": module exit\n");
}

module_init(day26_init);
module_exit(day26_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richer Wong");
MODULE_DESCRIPTION("Day26 EDU userspace-friendly tool driver");

/*
 * ==================== 附录：完整数据流图 ====================
 *
 * 【中断触发数据流】
 *
 *   用户态                    驱动                    EDU 硬件
 *   ─────────────────────────────────────────────────────────────
 *   write("1")
 *       │
 *       ↓
 *   syscall write() ────→ day26_write()
 *                              │
 *                              ├→ copy_from_user()
 *                              ├→ simple_strtoul()
 *                              ├→ 验证 v != 0
 *                              │
 *                              ↓
 *                         writel(v, BAR0+IRQ_RAISE)
 *                              │
 *                              ↓
 *                         [MSI 写内存特殊地址]
 *                              │
 *                              ↓
 *                              ├──────────────────→ EDU 设备
 *                                                         │
 *                                                         ↓
 *                                               MSI 中断触发
 *                                                         │
 *                              day26_irq_handler() ←─────┘
 *                              (CPU 接收中断)
 *                                      │
 *                                      ├→ readl(BAR0+IRQ_STATUS)
 *                                      ├→ irq_count++
 *                                      ├→ writel(status, BAR0+IRQ_ACK)
 *                                      └→ return IRQ_HANDLED
 *
 * 【ioctl GET_INFO 数据流】
 *
 *   用户态                    驱动
 *   ─────────────────────────────────
 *   ioctl(GET_INFO)
 *       │
 *       ↓
 *   syscall ioctl() ────→ day26_ioctl()
 *                              │
 *                              ├→ 构建 struct day26_info
 *                              │     (读取 d->pdev->vendor 等)
 *                              │
 *                              ├→ copy_to_user()
 *                              └→ return 0
 *       │
 *       ↓
 *   用户态收到 info 结构体
 *
 * 【read() 文本状态数据流】
 *
 *   用户态                    驱动
 *   ─────────────────────────────────
 *   read()
 *       │
 *       ↓
 *   syscall read() ────→ day26_read()
 *                              │
 *                              ├→ day26_build_state_text()
 *                              │     (生成多行文本)
 *                              │
 *                              ├→ simple_read_from_buffer()
 *                              │     (拷贝到用户态)
 *                              └→ return len
 *       │
 *       ↓
 *   用户态打印文本状态
 */
