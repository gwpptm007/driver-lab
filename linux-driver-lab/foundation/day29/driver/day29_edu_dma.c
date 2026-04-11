// SPDX-License-Identifier: GPL-2.0
/*
 * day29_edu_dma.c - QEMU EDU 一致性 DMA 往返验证驱动
 *
 * ==================== 文件概述 ====================
 *
 * Day29 是 W5 的起点，正式进入 DMA（Direct Memory Access）学习。
 *
 * 【学习焦点】
 *   1. DMA API：dma_set_mask_and_coherent()、dma_alloc_coherent()
 *   2. DMA 地址 vs 虚拟地址：两者不是同一个东西
 *   3. EDU DMA 寄存器：SRC、DST、COUNT、CMD
 *   4. 往返两次 DMA：RAM → EDU 内部 buffer → RAM
 *   5. 内核内验证：逐字节比较 src 和 dst
 *
 * 【与 W4 (Day27) 的核心区别】
 *   Day27：只做 MSI 中断，驱动是被动响应
 *   Day29：主动发起 DMA 传输，驱动是主动搬运数据
 *
 * 【硬件模型】
 *   继续使用 QEMU EDU 教学设备 (1234:11e8)
 *   - BAR0 MMIO：中断寄存器 + 新增 DMA 寄存器
 *   - DMA 控制器：EDU 内部 buffer 作为中转
 *   - DMA mask：32-bit（Day29 自动化放宽）
 *
 * ==================== 代码结构 ====================
 *
 *  1. 全局资源和模块参数
 *  2. EDU MMIO 读写封装（readl/writel）
 *  3. DMA 中断处理函数
 *  4. DMA 轮询等待函数
 *  5. DMA 编程函数
 *  6. 模式填充和验证结果重置
 *  7. DMA 往返验证主函数
 *  8. 文本状态快照生成
 *  9. file_operations（open/read/ioctl）
 * 10. 字符设备注册/销毁
 * 11. PCI probe/remove（包含 DMA 相关资源）
 * 12. 模块 init/exit
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "include/day29_edu_dma.h"
#include "../include/day29_edu_uapi.h"

/*
 * ==================== 第1部分：全局资源和模块参数 ====================
 *
 * 【dma_mask_bits 模块参数】
 *
 * 作用：允许运行时指定 DMA mask 位数
 *
 * 为什么需要？
 *   → QEMU EDU 默认只支持 28-bit DMA
 *   → arm64 virt 下 guest RAM 可能不在 28-bit 可达范围
 *   → Day29 自动化默认值 = 32-bit（通过 -device edu,dma_mask=0xffffffff）
 *   → 保留模块参数是为了手工实验时方便调整
 *
 * 【设计决定】
 *   早期版本通过 insmod 传 dma_mask_bits=32，但现场出现参数传递链路
 *   不稳定、最终被内核解析为空值的情况。因此默认值直接收口为 32，
 *   自动化流程不再依赖运行时模块参数。
 */
static dev_t g_day29_base_dev;
static struct class *g_day29_class;
static atomic_t g_day29_minor = ATOMIC_INIT(0);

static unsigned int dma_mask_bits = DAY29_DMA_MASK_BITS_DEFAULT;
module_param(dma_mask_bits, uint, 0644);
MODULE_PARM_DESC(dma_mask_bits, "DMA mask bits for QEMU EDU (default 32 in day29 automation; may be overridden manually for experiments)");

/*
 * ==================== 第2部分：EDU MMIO 读写封装 ====================
 *
 * 【32-bit vs 64-bit 访问】
 *   BAR0 寄存器大多是 32-bit，用 readl/writel
 *   DMA 源/目的地址是 64-bit，用 readq/writeq
 *   QEMU EDU 的 DMA 地址是 64-bit 宽
 */
static inline u32 day29_read32(struct day29_dev *d, u32 off)
{
    return readl(d->bar0 + off);
}

static inline void day29_write32(struct day29_dev *d, u32 off, u32 val)
{
    writel(val, d->bar0 + off);
}

static inline u64 day29_read64(struct day29_dev *d, u32 off)
{
    return readq(d->bar0 + off);
}

static inline void day29_write64(struct day29_dev *d, u32 off, u64 val)
{
    writeq(val, d->bar0 + off);
}

/*
 * ==================== 第3部分：DMA 中断处理函数 ====================
 *
 * 与 Day27 基本相同，IRQ handler 记录中断次数。
 *
 * 【注意】
 *   Day29 的 DMA 完成主要靠轮询 CMD 寄存器判断，不依赖中断。
 *   但 DMA_CMD 设置了 IRQ 位，所以 DMA 完成后会触发 IRQ。
 *   IRQ handler 仍然记录中断次数（irq_delta），用于验证。
 */
static irqreturn_t day29_irq_handler(int irq, void *opaque)
{
    struct day29_dev *d = opaque;
    unsigned long flags;
    u32 status;

    status = day29_read32(d, DAY29_EDU_REG_IRQ_STATUS);
    if (!status)
        return IRQ_NONE;

    spin_lock_irqsave(&d->irq_lock, flags);
    d->irq_count++;
    d->last_irq_status = status;
    d->last_ack_value = status;
    spin_unlock_irqrestore(&d->irq_lock, flags);

    day29_write32(d, DAY29_EDU_REG_IRQ_ACK, status);
    dev_info(&d->pdev->dev,
             "irq handler: irq=%d status=0x%08x count=%llu\n",
             irq, status, d->irq_count);
    return IRQ_HANDLED;
}

/*
 * ==================== 第4部分：DMA 轮询等待函数 ====================
 *
 * 【为什么需要轮询？】
 *   → Day29 的 DMA 完成判断以 command bit0 清零为准
 *   → IRQ 触发后，DMA 可能还没完全结束
 *   → 轮询确保 DMA 真正完成后再返回
 *
 * 【超时设计】
 *   旧版：10000 * 10us = 100ms
 *   新版：50000 * 10us = 500ms
 *   原因：实测第二段 DMA 在极限情况下会稍慢于 100ms
 *         导致"IRQ 已到，但仍被判成超时"的误判
 */
static int day29_wait_dma_idle(struct day29_dev *d)
{
    int i;
    u32 cmd;

    for (i = 0; i < 50000; ++i) {
        cmd = day29_read32(d, DAY29_EDU_REG_DMA_CMD);
        if (!(cmd & DAY29_EDU_DMA_CMD_START))
            return 0;  /* DMA 完成 */
        udelay(10);
    }

    dev_err(&d->pdev->dev, "DMA timeout: cmd=0x%08x\n",
            day29_read32(d, DAY29_EDU_REG_DMA_CMD));
    return -ETIMEDOUT;
}

/*
 * ==================== 第5部分：DMA 编程函数 ====================
 *
 * 【参数】
 *   src/dst：DMA 地址（不是虚拟地址！）
 *   count：传输字节数
 *   cmd：命令值（START、DIR、IRQ 等位的组合）
 *
 * 【为什么地址参数是 u64？】
 *   → EDU DMA 寄存器是 64-bit
 *   → DMA 地址可能是 32-bit 或 64-bit
 *   → 用 u64 确保能容纳所有情况
 *
 * 【关键】
 *   这里写入 DMA_SRC/DMA_DST 的是 DMA 地址（设备可见），
 *   不是 CPU 虚拟地址！
 */
static int day29_program_dma(struct day29_dev *d, u64 src, u64 dst,
                             u32 count, u32 cmd)
{
    if (!count)
        return -EINVAL;

    d->last_dma_cmd = cmd;
    day29_write64(d, DAY29_EDU_REG_DMA_SRC, src);    /* 源地址 */
    day29_write64(d, DAY29_EDU_REG_DMA_DST, dst);    /* 目的地址 */
    day29_write32(d, DAY29_EDU_REG_DMA_COUNT, count); /* 字节数 */
    day29_write32(d, DAY29_EDU_REG_DMA_CMD, cmd);    /* 命令 */
    return day29_wait_dma_idle(d);                   /* 等待完成 */
}

/*
 * ==================== 第6部分：模式填充和验证结果重置 ====================
 *
 * 【fill_pattern】
 *   填充模式：byte[i] = (seed + i) & 0xFF
 *   用于 DMA 往返验证，确保数据有规律可循
 *
 * 【reset_verify_result】
 *   在每次验证前重置结果字段
 *   确保新验证不受旧数据影响
 */
static void day29_fill_pattern(u8 *buf, u32 len, u32 seed)
{
    u32 i;
    for (i = 0; i < len; ++i)
        buf[i] = (u8)((seed + i) & 0xff);
}

static void day29_reset_verify_result(struct day29_dev *d)
{
    d->last_verify_len = 0;
    d->last_verify_seed = 0;
    d->last_verify_error = 0;
    d->last_verify_ok = 0;
    d->last_mismatch_index = -1;
    d->last_mismatch_expected = 0;
    d->last_mismatch_actual = 0;
    d->last_irq_delta = 0;
    d->last_dma_cmd = 0;
}

/*
 * ==================== 第7部分：DMA 往返验证主函数 ====================
 *
 * 【核心流程】
 *   1. 参数校验（len > 0, len <= 2048, dma_virt != NULL）
 *   2. 获取 src/dst 虚拟地址和 DMA 地址
 *   3. 清零 dst，填充 src 模式
 *   4. 第一次 DMA：RAM(src) → EDU(0x40000)
 *   5. 第二次 DMA：EDU(0x40000) → RAM(dst)
 *   6. 比较 src 和 dst
 *
 * 【为什么要用 mutex？】
 *   → DMA 操作不能并发
 *   → 一次 DMA 往返必须完整执行完
 *   → mutex 确保操作原子性
 *
 * 【为什么在内核比较？】
 *   → Day29 的目标是学习 DMA API
 *   → 把验证放内核逻辑最短、最容易定位问题
 *   → Day30 会逐步把比较移到用户态
 *
 * 【往返 DMA 图解】
 *   coherent buffer
 *   ┌─────────────────────────────────────────────┐
 *   │ src [0~2047]      │ dst [2048~4095]        │
 *   │ 0x41,0x42,...     │ (清零后)               │
 *   └─────────────────────────────────────────────┘
 *           ↑                           │
 *           │                           │
 *           │ 2nd DMA                  │ 1st DMA
 *           │ (EDU→RAM)               │ (RAM→EDU)
 *           │                           ↓
 *           │               ┌───────────────────┐
 *           │               │ EDU internal buf  │
 *           │               │ (偏移 0x40000)    │
 *           │               └───────────────────┘
 *           │                           ↑
 *           └───────────────────────────┘
 */
static int day29_do_verify(struct day29_dev *d, u32 len, u32 seed)
{
    u8 *src;
    u8 *dst;
    u64 src_dma;
    u64 dst_dma;
    u64 irq_before;
    int ret;
    u32 i;

    /* 校验 buffer 存在 */
    if (!d->dma_virt)
        return -ENODEV;

    /* 校验长度（最大 2048，buffer 一半） */
    if (!len || len > DAY29_DMA_VERIFY_MAX)
        return -EINVAL;

    /* 加锁：DMA 操作不能并发 */
    mutex_lock(&d->op_lock);

    /* 重置验证结果 */
    day29_reset_verify_result(d);
    d->last_verify_len = len;
    d->last_verify_seed = seed;

    /* 计算 src/dst 的虚拟地址和 DMA 地址 */
    src = (u8 *)d->dma_virt + DAY29_DMA_SRC_OFF;       /* src 虚拟地址 */
    dst = (u8 *)d->dma_virt + DAY29_DMA_DST_OFF;        /* dst 虚拟地址 */
    src_dma = (u64)d->dma_handle + DAY29_DMA_SRC_OFF;   /* src DMA 地址 */
    dst_dma = (u64)d->dma_handle + DAY29_DMA_DST_OFF;    /* dst DMA 地址 */

    /* 清零 dst，填充 src 模式 */
    memset(d->dma_virt, 0, d->dma_bytes);
    day29_fill_pattern(src, len, seed);

    /* 记录验证前的 IRQ 计数（用于计算 irq_delta） */
    irq_before = d->irq_count;

    dev_info(&d->pdev->dev,
             "verify start: len=%u seed=0x%x src_dma=0x%llx dst_dma=0x%llx\n",
             len, seed,
             (unsigned long long)src_dma,
             (unsigned long long)dst_dma);

    /* 第一次 DMA：RAM → EDU */
    ret = day29_program_dma(d, src_dma, DAY29_EDU_DEVBUF_OFFSET,
                            len,
                            DAY29_EDU_DMA_CMD_START |
                            DAY29_EDU_DMA_CMD_IRQ);
    if (ret) {
        d->last_verify_error = ret;
        dev_err(&d->pdev->dev, "verify stage1 RAM->EDU failed: %d\n", ret);
        goto out;
    }

    /* 第二次 DMA：EDU → RAM */
    ret = day29_program_dma(d, DAY29_EDU_DEVBUF_OFFSET, dst_dma,
                            len,
                            DAY29_EDU_DMA_CMD_START |
                            DAY29_EDU_DMA_CMD_DIR_TO_RAM |
                            DAY29_EDU_DMA_CMD_IRQ);
    if (ret) {
        d->last_verify_error = ret;
        dev_err(&d->pdev->dev, "verify stage2 EDU->RAM failed: %d\n", ret);
        goto out;
    }

    /* 计算验证期间的 IRQ 增量（应该是 2） */
    d->last_irq_delta = (u32)(d->irq_count - irq_before);

    /* 逐字节比较 src 和 dst */
    for (i = 0; i < len; ++i) {
        if (src[i] != dst[i]) {
            /* 记录第一个 mismatch 的位置和值 */
            d->last_verify_error = -EIO;
            d->last_mismatch_index = (s32)i;
            d->last_mismatch_expected = src[i];
            d->last_mismatch_actual = dst[i];
            dev_err(&d->pdev->dev,
                    "verify mismatch: idx=%u expected=0x%02x actual=0x%02x\n",
                    i, src[i], dst[i]);
            goto out;
        }
    }

    /* 验证成功 */
    d->last_verify_ok = 1;
    dev_info(&d->pdev->dev,
             "verify ok: len=%u seed=0x%x irq_delta=%u\n",
             len, seed, d->last_irq_delta);

out:
    mutex_unlock(&d->op_lock);
    return d->last_verify_error;
}

/*
 * ==================== 第8部分：文本状态快照生成 ====================
 *
 * 【与 Day27 的区别】
 *   Day29 的状态输出包含 DMA 相关字段：
 *   - dma_handle：DMA 地址
 *   - dma_bytes：buffer 大小
 *   - dma_mask_bits：DMA mask 位数
 *   - verify_*：最近一次验证结果
 */
static ssize_t day29_build_state_text(struct day29_dev *d, char *buf, size_t size)
{
    return scnprintf(buf, size,
                     "vendor=0x%04x device=0x%04x\n"
                     "bar0_start=0x%llx bar0_len=0x%llx\n"
                     "irq_vector=%u irq_count=%llu msi_enabled=%u\n"
                     "last_irq_status=0x%08x last_ack_value=0x%08x\n"
                     "dma_handle=0x%llx dma_bytes=%zu dma_mask_bits=%u\n"
                     "verify_len=%u verify_seed=0x%x verify_ok=%u verify_error=%d\n"
                     "mismatch_index=%d mismatch_expected=0x%02x mismatch_actual=0x%02x\n"
                     "last_irq_delta=%u last_dma_cmd=0x%08x\n",
                     d->pdev->vendor,
                     d->pdev->device,
                     (unsigned long long)d->bar0_start,
                     (unsigned long long)d->bar0_len,
                     d->irq_vector,
                     d->irq_count,
                     !!(d->pdev->msi_enabled),
                     d->last_irq_status,
                     d->last_ack_value,
                     (unsigned long long)d->dma_handle,
                     d->dma_bytes,
                     d->dma_mask_bits,
                     d->last_verify_len,
                     d->last_verify_seed,
                     d->last_verify_ok,
                     d->last_verify_error,
                     d->last_mismatch_index,
                     d->last_mismatch_expected,
                     d->last_mismatch_actual,
                     d->last_irq_delta,
                     d->last_dma_cmd);
}

/*
 * ==================== 第9部分：file_operations ====================
 *
 * 【open】
 *   与 Day27 相同，通过 container_of 获取 day29_dev
 */
static int day29_open(struct inode *inode, struct file *file)
{
    struct day29_dev *d = container_of(inode->i_cdev, struct day29_dev, cdev);
    file->private_data = d;
    return 0;
}

/*
 * 【read】
 *   返回文本状态快照，与 Day27 相同
 */
static ssize_t day29_read(struct file *file, char __user *buf,
                          size_t count, loff_t *ppos)
{
    struct day29_dev *d = file->private_data;
    char kbuf[320];
    ssize_t len;

    if (!d || !d->bar0)
        return -ENODEV;

    len = day29_build_state_text(d, kbuf, sizeof(kbuf));
    return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

/*
 * 【ioctl】
 *   Day29 的主操作面：
 *   - GET_INFO：完整状态
 *   - RUN_VERIFY：触发 DMA 往返验证
 *   - GET_VERIFY_RESULT：获取验证结果
 *   - RESET_STATS：重置统计
 */
static long day29_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct day29_dev *d = file->private_data;

    switch (cmd) {

    case DAY29_IOC_GET_INFO: {
        struct day29_info info = {
            .tool_api_version = DAY29_TOOL_API_VERSION,
            .vendor_id = d->pdev->vendor,
            .device_id = d->pdev->device,
            .irq_vector = d->irq_vector,
            .irq_count = d->irq_count,
            .last_irq_status = d->last_irq_status,
            .last_ack_value = d->last_ack_value,
            .bar0_start = d->bar0_start,
            .bar0_len = d->bar0_len,
            .dma_handle = d->dma_handle,
            .dma_bytes = d->dma_bytes,
            .dma_mask_bits = d->dma_mask_bits,
            .msi_enabled = !!(d->pdev->msi_enabled),
            .last_verify_len = d->last_verify_len,
            .last_verify_seed = d->last_verify_seed,
            .last_verify_ok = d->last_verify_ok,
            .last_verify_error = d->last_verify_error,
            .last_mismatch_index = d->last_mismatch_index,
            .last_mismatch_expected = d->last_mismatch_expected,
            .last_mismatch_actual = d->last_mismatch_actual,
            .last_irq_delta = d->last_irq_delta,
            .last_dma_cmd = d->last_dma_cmd,
        };
        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;
    }

    case DAY29_IOC_RUN_VERIFY: {
        struct day29_verify_req req;
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;
        return day29_do_verify(d, req.len, req.pattern_seed);
    }

    case DAY29_IOC_GET_VERIFY_RESULT: {
        struct day29_verify_result res = {
            .verify_len = d->last_verify_len,
            .verify_seed = d->last_verify_seed,
            .verify_ok = d->last_verify_ok,
            .verify_error = d->last_verify_error,
            .mismatch_index = d->last_mismatch_index,
            .mismatch_expected = d->last_mismatch_expected,
            .mismatch_actual = d->last_mismatch_actual,
            .irq_delta = d->last_irq_delta,
            .last_dma_cmd = d->last_dma_cmd,
        };
        if (copy_to_user((void __user *)arg, &res, sizeof(res)))
            return -EFAULT;
        return 0;
    }

    case DAY29_IOC_RESET_STATS:
        mutex_lock(&d->op_lock);
        d->irq_count = 0;
        d->last_irq_status = 0;
        d->last_ack_value = 0;
        day29_reset_verify_result(d);
        mutex_unlock(&d->op_lock);
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations day29_fops = {
    .owner          = THIS_MODULE,
    .open           = day29_open,
    .read           = day29_read,
    .unlocked_ioctl = day29_ioctl,
    .llseek         = no_llseek,
};

/*
 * ==================== 第10部分：字符设备注册/销毁 ====================
 *
 * 与 Day27 基本相同，略。
 */
static int day29_setup_chrdev(struct day29_dev *d)
{
    int minor;
    int ret;

    minor = atomic_fetch_add(1, &g_day29_minor);
    d->devt = MKDEV(MAJOR(g_day29_base_dev), minor);

    cdev_init(&d->cdev, &day29_fops);
    d->cdev.owner = THIS_MODULE;

    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret)
        return ret;

    d->device = device_create(g_day29_class, &d->pdev->dev, d->devt, NULL,
                              DAY29_DEV_NAME_FMT, minor);
    if (IS_ERR(d->device)) {
        ret = PTR_ERR(d->device);
        d->device = NULL;
        cdev_del(&d->cdev);
        return ret;
    }
    return 0;
}

static void day29_destroy_chrdev(struct day29_dev *d)
{
    if (d->device)
        device_destroy(g_day29_class, d->devt);
    cdev_del(&d->cdev);
}

/*
 * ==================== 第11部分：PCI probe（核心 DMA bring-up）====================
 *
 * 【probe 中的新步骤（相比 Day27）】
 *   1. dma_set_mask_and_coherent()    ← 新增
 *   2. dma_alloc_coherent()            ← 新增
 *
 * 【error label 新增】
 *   err_dma: dma_free_coherent()       ← 新增
 */
static int day29_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day29_dev *d;
    u32 ident;
    u32 live;
    int ret;

    dev_info(&pdev->dev, "probe enter: %04x:%04x\n", pdev->vendor, pdev->device);

    /* 分配私有数据 */
    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;

    d->pdev = pdev;
    d->dma_mask_bits = dma_mask_bits;     /* 从模块参数获取 */
    d->dma_bytes = DAY29_DMA_BYTES;       /* 4096 */
    d->last_mismatch_index = -1;
    spin_lock_init(&d->irq_lock);
    mutex_init(&d->op_lock);              /* 初始化 DMA 操作锁 */
    pci_set_drvdata(pdev, d);

    /* 启用 PCI 设备 */
    ret = pci_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "pci_enable_device failed: %d\n", ret);
        goto err_free;
    }

    /*
     * 【关键】设置 DMA mask
     *
     * 告诉设备驱动我们支持多宽的 DMA 地址。
     * 这里用模块参数指定（默认 32-bit）。
     *
     * 如果失败，说明设备或平台不支持这个 DMA width。
     */
    ret = dma_set_mask_and_coherent(&pdev->dev,
                                    DMA_BIT_MASK(d->dma_mask_bits));
    if (ret) {
        dev_err(&pdev->dev,
                "dma_set_mask_and_coherent(%u bits) failed: %d\n",
                d->dma_mask_bits, ret);
        goto err_disable;
    }
    dev_info(&pdev->dev, "dma mask set to %u bits\n", d->dma_mask_bits);

    /* 请求 BAR 资源 */
    ret = pci_request_regions(pdev, DAY29_DRV_NAME);
    if (ret) {
        dev_err(&pdev->dev, "pci_request_regions failed: %d\n", ret);
        goto err_disable;
    }

    /* 设置为主设备 */
    pci_set_master(pdev);

    /* 映射 BAR0 */
    d->bar0_start = pci_resource_start(pdev, 0);
    d->bar0_len = pci_resource_len(pdev, 0);
    dev_info(&pdev->dev, "BAR0: start=0x%llx len=0x%llx flags=0x%lx\n",
             (unsigned long long)d->bar0_start,
             (unsigned long long)d->bar0_len,
             (unsigned long)pci_resource_flags(pdev, 0));

    d->bar0 = pci_iomap(pdev, 0, 0);
    if (!d->bar0) {
        ret = -ENOMEM;
        dev_err(&pdev->dev, "BAR0: pci_iomap failed\n");
        goto err_regions;
    }

    /* 读取设备标识（验证连接） */
    ident = day29_read32(d, DAY29_EDU_REG_IDENTITY);
    live = day29_read32(d, DAY29_EDU_REG_LIVENESS);
    dev_info(&pdev->dev, "ident=0x%08x liveness=0x%08x\n", ident, live);

    /*
     * 【核心】分配一致性 DMA buffer
     *
     * 返回两个地址：
     *   - d->dma_virt：CPU 访问用的虚拟地址
     *   - d->dma_handle：设备访问用的 DMA 地址
     *
     * coherent 的含义：
     *   - CPU 写入后，设备立即能看到（无需 cache flush）
     *   - 设备写入后，CPU 立即能看到（无需 cache invalidate）
     */
    d->dma_virt = dma_alloc_coherent(&pdev->dev, d->dma_bytes,
                                     &d->dma_handle, GFP_KERNEL);
    if (!d->dma_virt) {
        ret = -ENOMEM;
        dev_err(&pdev->dev, "dma_alloc_coherent(%zu) failed\n", d->dma_bytes);
        goto err_iounmap;
    }
    memset(d->dma_virt, 0, d->dma_bytes);
    dev_info(&pdev->dev,
             "dma_alloc_coherent ok: virt=%px dma=0x%llx bytes=%zu\n",
             d->dma_virt,
             (unsigned long long)d->dma_handle,
             d->dma_bytes);

    /* 分配 MSI 中断 */
    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
    if (ret < 0) {
        dev_err(&pdev->dev, "pci_alloc_irq_vectors failed: %d\n", ret);
        goto err_dma;
    }
    d->irq_vector = pci_irq_vector(pdev, 0);

    /* 注册中断处理函数 */
    ret = request_irq(d->irq_vector, day29_irq_handler, 0, DAY29_DRV_NAME, d);
    if (ret) {
        dev_err(&pdev->dev, "request_irq(%u) failed: %d\n", d->irq_vector, ret);
        goto err_irq_vectors;
    }
    dev_info(&pdev->dev, "MSI vector=%u enabled=%u\n",
             d->irq_vector, !!pdev->msi_enabled);

    /* 注册字符设备 */
    ret = day29_setup_chrdev(d);
    if (ret) {
        dev_err(&pdev->dev, "day29_setup_chrdev failed: %d\n", ret);
        goto err_irq;
    }

    dev_info(&pdev->dev, "probe success\n");
    return 0;

/* 错误处理（按分配倒序） */
err_irq:
    free_irq(d->irq_vector, d);
err_irq_vectors:
    pci_free_irq_vectors(pdev);
err_dma:
    /* 释放 DMA buffer */
    dma_free_coherent(&pdev->dev, d->dma_bytes, d->dma_virt, d->dma_handle);
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
 * ==================== 第12部分：PCI remove（对称释放）====================
 *
 * 【与 Day27 的区别】
 *   remove 中新增：dma_free_coherent()
 */
static void day29_remove(struct pci_dev *pdev)
{
    struct day29_dev *d = pci_get_drvdata(pdev);

    if (!d)
        return;

    dev_info(&pdev->dev, "remove enter\n");

    /* 销毁字符设备 */
    day29_destroy_chrdev(d);

    /* 释放中断 */
    free_irq(d->irq_vector, d);
    pci_free_irq_vectors(pdev);

    /* 释放 DMA buffer */
    if (d->dma_virt)
        dma_free_coherent(&pdev->dev, d->dma_bytes, d->dma_virt, d->dma_handle);

    /* 释放 MMIO 映射 */
    if (d->bar0)
        pci_iounmap(pdev, d->bar0);

    /* 释放 BAR 资源 */
    pci_release_regions(pdev);

    /* 禁用 PCI 设备 */
    pci_disable_device(pdev);

    /* 释放私有数据 */
    kfree(d);

    dev_info(&pdev->dev, "remove leave\n");
}

/*
 * ==================== 第13部分：pci_driver 和模块 init/exit ============
 *
 * 与 Day27 基本相同，略。
 */
static const struct pci_device_id day29_pci_ids[] = {
    { PCI_DEVICE(DAY29_EDU_VENDOR_ID, DAY29_EDU_DEVICE_ID) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, day29_pci_ids);

static struct pci_driver day29_pci_driver = {
    .name = DAY29_DRV_NAME,
    .id_table = day29_pci_ids,
    .probe = day29_probe,
    .remove = day29_remove,
};

static int __init day29_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&g_day29_base_dev, 0, 256, DAY29_DRV_NAME);
    if (ret)
        return ret;

    g_day29_class = class_create(THIS_MODULE, DAY29_CLASS_NAME);
    if (IS_ERR(g_day29_class)) {
        ret = PTR_ERR(g_day29_class);
        unregister_chrdev_region(&g_day29_base_dev, 256);
        return ret;
    }

    ret = pci_register_driver(&day29_pci_driver);
    if (ret) {
        class_destroy(g_day29_class);
        unregister_chrdev_region(&g_day29_base_dev, 256);
        return ret;
    }

    pr_info(DAY29_DRV_NAME ": init ok\n");
    return 0;
}

static void __exit day29_exit(void)
{
    pci_unregister_driver(&day29_pci_driver);
    class_destroy(g_day29_class);
    unregister_chrdev_region(&g_day29_base_dev, 256);
    pr_info(DAY29_DRV_NAME ": exit ok\n");
}

module_init(day29_init);
module_exit(day29_exit);

MODULE_AUTHOR("Richer Wong");
MODULE_DESCRIPTION("Day29 QEMU EDU coherent DMA round-trip driver");
MODULE_LICENSE("GPL");

/*
 * ==================== 附录：DMA 地址 vs 虚拟地址 ====================
 *
 * 【常见误解】
 *   错误：把 dma_virt 当成 dma_handle 写入 DMA 寄存器
 *   正确：把 dma_handle 写入 DMA 寄存器
 *
 * 【为什么不能混用？】
 *   - CPU 用虚拟地址访问内存（MMU 转换）
 *   - 设备用 DMA 地址（通常是物理地址或总线地址）
 *   - DMA 地址是设备能访问的地址空间
 *
 * 【图示】
 *   CPU                         设备
 *    │                           │
 *    │ 虚拟地址                  │ DMA 地址
 *    ↓                           ↓
 *   ┌──────────────────────────────┐
 *   │       MMU 转换               │ 设备直接访问
 *   └──────────────────────────────┘
 *                   ↓
 *              物理地址
 *
 * 【dma_alloc_coherent 的作用】
 *   - 分配一块 DMA 可访问的内存
 *   - 返回两个地址：虚拟地址（CPU 用）和 DMA 地址（设备用）
 *   - 保证两者映射到同一块物理内存
 */
