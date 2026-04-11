// SPDX-License-Identifier: GPL-2.0
/*
 * day34_edu_stability.c - Day34 QEMU EDU 稳定性测试驱动
 *
 * ==================== 驱动作用 ====================
 *
 * Day34 延续 day33 的 coherent DMA + mmap + RUN_DMA 基线，
 * 但目标从 trace/bench 转移到三个稳定性场景：
 *   1. 多进程并发访问同一字符设备
 *   2. insmod/rmmod 生命周期循环
 *   3. 非法长度与非法 mmap offset 错误注入
 *
 * 驱动本身不追求新功能，而是提供：
 *   - 稳定的 mmap + RUN_DMA 数据路径
 *   - 可回读的最近一次运行结果
 *   - 明确的边界拒绝
 *
 * ==================== 与 Day33 的区别 ====================
 *
 *   Day33：ftrace 看清调用路径
 *   Day34：在功能验证基础上做压力测试
 *
 *   驱动代码本身没有本质变化，区别在于：
 *     - 模块参数名：trace_verbose → stability_verbose
 *     - 新增 irq_vectors_allocated 和 irq_requested 标志
 *     - 使用 request_irq()/free_irq() 手动配对
 *     - 新增错误注入拒绝逻辑
 *
 * ==================== 代码结构 ====================
 *
 *  第1部分：头文件依赖
 *  第2部分：模块参数和全局变量
 *  第3部分：寄存器访问辅助函数
 *  第4部分：中断处理（IRQ_HANDLER）
 *  第5部分：DMA 编程与等待（PROGRAM_DMA / WAIT_IDLE）
 *  第6部分：结果记录辅助函数
 *  第7部分：主 DMA 执行逻辑（DO_RUN_DMA）
 *  第8部分：file_operations 回调
 *  第9部分：PCI probe/remove
 *  第10部分：模块初始化/退出
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/bitops.h>

#include "include/day34_edu_stability.h"
#include "../include/day34_edu_uapi.h"

/*
 * ==================== 第1部分：头文件依赖 ====================
 *
 * Day34 新增的头文件：
 *   linux/delay.h  → udelay() 用于 DMA 轮询等待
 *   linux/bitops.h → likely()/unlikely() 分支预测优化
 *   linux/types.h  → bool 类型（在 day34_edu_stability.h 中使用）
 *
 * 其他头文件与 Day33 相同：
 *   linux/cdev.h    → cdev_init/cdev_add（字符设备）
 *   linux/device.h → class_create/device_create（设备模型）
 *   linux/dma-mapping.h → dma_set_mask_and_coherent/dma_alloc_coherent
 *   linux/fs.h     → struct file_operations
 *   linux/interrupt.h → request_irq/free_irq/irqreturn_t
 *   linux/io.h      → readl/writel/writeq（MMIO 访问）
 *   linux/kernel.h  → struct device（dev_info/dev_err）
 *   linux/mm.h      → struct vm_area_struct（mmap）
 *   linux/module.h  → MODULE_LICENSE/MODULE_DEVICE_TABLE
 *   linux/pci.h     → pci_* 系列函数
 *   linux/slab.h    → devm_kzalloc
 *   linux/uaccess.h → copy_from_user/copy_to_user
 *   linux/ktime.h   → ktime_get_ns()（纳秒级计时）
 */

/*
 * ==================== 第2部分：模块参数和全局变量 ====================
 *
 * 【模块参数：stability_verbose】
 *
 * Day34 新增 stability_verbose 模块参数：
 *
 *   insmod day34_edu_stability.ko              # 默认，关闭热路径日志
 *   insmod day34_edu_stability.ko stability_verbose=1  # 开启，用于调试
 *
 * 为什么要关闭？
 *   stability_verbose=true 时，每次 IRQ 都会 dev_info() 打印，
 *   1000 次模块循环 + 并发压测会产生海量输出，拖慢测试。
 *
 * 为什么叫 stability_verbose？
 *   Day34 主题是稳定性测试（与 Day33 的 trace_verbose 语义对应）。
 *
 * 【全局变量】
 *
 *   g_day34_base_dev：字符设备号范围（alloc_chrdev_region）
 *   g_day34_class：sysfs 类（class_create）
 *   g_day34_minor：次设备号计数器（atomic_t，支持多设备实例）
 */
static dev_t g_day34_base_dev;
static struct class *g_day34_class;
static atomic_t g_day34_minor = ATOMIC_INIT(0);

static unsigned int dma_mask_bits = DAY34_DMA_MASK_BITS_DEFAULT;
module_param(dma_mask_bits, uint, 0644);
MODULE_PARM_DESC(dma_mask_bits, "DMA mask bits for QEMU EDU (default 32)");

static bool stability_verbose;
module_param(stability_verbose, bool, 0644);
MODULE_PARM_DESC(stability_verbose, "Enable verbose hot-path logging (default false)");

/*
 * ==================== 第3部分：寄存器访问辅助函数 ====================
 *
 * 与 Day33 完全相同：
 *   day34_read32()  → 读 32 位 MMIO 寄存器
 *   day34_write32() → 写 32 位 MMIO 寄存器
 *   day34_write64() → 写 64 位 MMIO 寄存器（DMA_SRC/DMA_DST）
 *
 * 使用 readl/writel/writeq 而不是 ioread32/iowrite32：
 *   - pcim_iomap_table() 返回的是直接映射的虚拟地址（不是 iomem）
 *   - 可以直接使用 readl/writel
 */
static inline u32 day34_read32(struct day34_dev *d, u32 off)
{
    return readl(d->bar0 + off);
}

static inline void day34_write32(struct day34_dev *d, u32 off, u32 val)
{
    writel(val, d->bar0 + off);
}

static inline void day34_write64(struct day34_dev *d, u32 off, u64 val)
{
    writeq(val, d->bar0 + off);
}

/*
 * ==================== 第4部分：中断处理（IRQ_HANDLER） ====================
 *
 * 【与 Day33 的区别】
 *
 * Day33 的 day33_irq_handler：
 *   - 只是读取 IRQ_STATUS 并 ACK
 *   - 用于 ftrace 观察调用路径
 *
 * Day34 的 day34_irq_handler：
 *   - 同样的逻辑，但注释更强调稳定性
 *   - stability_verbose 控制是否打印
 *
 * 【为什么 IRQ handler 这么快？】
 *
 *   - QEMU EDU 是软件模拟，IRQ 只是内存写入
 *   - handler 只是读状态 + 写 ACK，不做复杂操作
 *   - 真实硬件的 IRQ handler 可能涉及等待、锁等
 */
static irqreturn_t day34_irq_handler(int irq, void *opaque)
{
    struct day34_dev *d = opaque;
    unsigned long flags;
    u32 status;

    // 读取 IRQ_STATUS，判断是否真的有中断待处理
    status = day34_read32(d, DAY34_EDU_REG_IRQ_STATUS);
    if (!status)
        return IRQ_NONE;  // 假阳性（spurious），不是我们的设备触发的

    // 使用 spinlock 保护 irq_count 和 last_irq_status
    //irq_lock 是 spinlock_t，在中断上下文中使用 spin_lock_irqsave
    spin_lock_irqsave(&d->irq_lock, flags);
    d->irq_count++;
    d->last_irq_status = status;
    d->last_ack_value = status;
    spin_unlock_irqrestore(&d->irq_lock, flags);

    // ACK 中断：向 IRQ_ACK 寄存器写入状态位
    day34_write32(d, DAY34_EDU_REG_IRQ_ACK, status);

    // 可选的 verbose 打印（默认关闭，1000 次循环会打印太多）
    if (unlikely(stability_verbose))
        dev_info(&d->pdev->dev, "irq handler: irq=%d status=0x%08x count=%llu",
                 irq, status, d->irq_count);
    return IRQ_HANDLED;
}

/*
 * ==================== 第5部分：DMA 编程与等待 ====================
 *
 * 【day34_wait_dma_idle：轮询等待 DMA 完成】
 *
 * 与 Day33 完全相同：
 *   - 轮询 DMA_CMD 寄存器的 START 位
 *   - 最多 50000 次，每次 udelay(10) = 10 微秒
 *   - 总超时：50000 * 10us = 500ms
 *
 * 【为什么使用轮询而不是睡眠？】
 *
 *   - DMA 传输在 QEMU EDU 中是软件模拟，瞬间完成
 *   - 轮询 50000 次总共只需 500ms，且大多数情况下几微秒就完成
 *   - 睡眠/唤醒机制的开销反而更大
 *
 * 【day34_program_dma：DMA 寄存器编程】
 *
 *   1. 检查 count 是否为 0（边界检查）
 *   2. 记录 last_dma_cmd（用于调试）
 *   3. 写入 DMA_SRC/DMA_DST/DMA_COUNT/DMA_CMD
 *   4. 等待 DMA 完成
 */
static int day34_wait_dma_idle(struct day34_dev *d)
{
    int i;
    u32 cmd;

    // 轮询 DMA_CMD.START 位
    for (i = 0; i < 50000; ++i) {
        cmd = day34_read32(d, DAY34_EDU_REG_DMA_CMD);
        if (!(cmd & DAY34_EDU_DMA_CMD_START))
            return 0;  // DMA 完成
        udelay(10);
    }

    // 超时：START 位一直为 1，说明设备没有正常完成 DMA
    dev_err(&d->pdev->dev, "DMA timeout: cmd=0x%08x", day34_read32(d, DAY34_EDU_REG_DMA_CMD));
    return -ETIMEDOUT;
}

static int day34_program_dma(struct day34_dev *d, u64 src, u64 dst, u32 count, u32 cmd)
{
    // 边界检查：count 不能为 0
    if (!count)
        return -EINVAL;

    // 记录 cmd 值（用于调试和回读）
    d->last_dma_cmd = cmd;

    // 编程 DMA 寄存器
    day34_write64(d, DAY34_EDU_REG_DMA_SRC, src);
    day34_write64(d, DAY34_EDU_REG_DMA_DST, dst);
    day34_write32(d, DAY34_EDU_REG_DMA_COUNT, count);
    day34_write32(d, DAY34_EDU_REG_DMA_CMD, cmd);

    // 等待 DMA 完成
    return day34_wait_dma_idle(d);
}

/*
 * ==================== 第6部分：结果记录辅助函数 ====================
 *
 * 【day34_reset_run_result：重置 RUN_DMA 结果】
 *
 * 在每次 RUN_DMA 开始前调用，重置以下字段：
 *   - last_run_ns / last_run_len / last_run_seed
 *   - last_run_error / last_run_ok / last_irq_delta / last_dma_cmd
 *
 * 确保"最近一次运行结果"反映当前这次，而不是上一次。
 *
 * 【day34_record_mmap_result：记录 mmap 结果】
 *
 * 用于记录最近一次 mmap 尝试的结果：
 *   - ok：是否成功
 *   - err：errno（失败时为负数）
 *   - len：请求的 length
 *   - pgoff：请求的 page offset
 *
 * 为什么需要这个？
 *   - mmap 是 file_operations 的一部分，返回值有限
 *   - 用户态通过 ioctl(GET_RESULT) 查询 mmap 结果
 *   - 错误注入测试需要验证 mmap 确实被拒绝
 */
static void day34_reset_run_result(struct day34_dev *d)
{
    d->last_run_ns = 0;
    d->last_run_len = 0;
    d->last_run_seed = 0;
    d->last_run_error = 0;
    d->last_run_ok = 0;
    d->last_irq_delta = 0;
    d->last_dma_cmd = 0;
}

static void day34_record_mmap_result(struct day34_dev *d, bool ok, int err,
                                     unsigned long len, unsigned long pgoff)
{
    d->last_mmap_ok = ok ? 1U : 0U;
    d->last_mmap_error = err;
    d->last_mmap_len = (u32)len;
    d->last_mmap_pgoff = (u32)pgoff;
}

/*
 * ==================== 第7部分：主 DMA 执行逻辑（DO_RUN_DMA） ====================
 *
 * 【day34_do_run_dma：两段 DMA 的完整流程】
 *
 *   Stage 1：RAM → EDU（填充 src 数据到 EDU 设备内存）
 *   Stage 2：EDU → RAM（把 EDU 内存的数据复制到 dst）
 *
 * 【与 Day33 的区别】
 *
 * Day34 的 do_run_dma 没有本质变化，但注释更强调：
 *   - 边界检查（len > DAY34_DMA_VERIFY_MAX）
 *   - 错误处理（total_run_fail 统计）
 *   - irq_delta 记录（用于验证 DMA 确实触发了两段 IRQ）
 *
 * 【性能考量】
 *
 *   - 使用 op_lock 互斥锁（保护 device state）
 *   - 使用 ktime_get_ns() 计时（纳秒级精度）
 *   - 结果写入 last_run_* 字段供用户态查询
 */
static int day34_do_run_dma(struct day34_dev *d, u32 len, u32 seed)
{
    u64 src_dma, dst_dma, irq_before, start_ns, end_ns;
    int ret;
    u8 *src, *dst;
    u32 i;

    // 边界检查：DMA buffer 未分配 或 len 超出验证范围
    if (!d->dma_virt)
        return -ENODEV;
    if (!len || len > DAY34_DMA_VERIFY_MAX)  // DAY34_DMA_VERIFY_MAX = 2048
        return -EINVAL;

    // 获取互斥锁（保护 device state：统计计数、last_run_*）
    mutex_lock(&d->op_lock);

    // 重置结果字段
    day34_reset_run_result(d);

    // 统计计数
    d->total_run_calls++;
    d->last_run_len = len;
    d->last_run_seed = seed;

    // 计算 src/dst 在 DMA buffer 中的虚拟地址
    src = (u8 *)d->dma_virt + DAY34_DMA_SRC_OFF;
    dst = (u8 *)d->dma_virt + DAY34_DMA_DST_OFF;

    // 填充 src：使用 pattern_seed 生成确定性的测试数据
    for (i = 0; i < len; ++i)
        src[i] = (u8)((seed + i) & 0xff);
    memset(dst, 0, len);

    // 计算 src/dst 的 DMA 地址（物理地址，用于 EDU 访问）
    src_dma = (u64)d->dma_handle + DAY34_DMA_SRC_OFF;
    dst_dma = (u64)d->dma_handle + DAY34_DMA_DST_OFF;

    // 记录 DMA 开始前的 irq_count（用于计算 irq_delta）
    irq_before = d->irq_count;

    // 记录开始时间（纳秒级）
    start_ns = ktime_get_ns();

    // 可选的 verbose 打印
    if (unlikely(stability_verbose))
        dev_info(&d->pdev->dev,
                 "run_dma start: len=%u seed=0x%x src_dma=0x%llx dst_dma=0x%llx",
                 len, seed, src_dma, dst_dma);

    /*
     * Stage 1：RAM → EDU
     *   src = DMA buffer 中的 src 区域（虚拟地址）
     *   dst = EDU 设备内存（固定偏移 0x40000）
     *   命令：START | IRQ（完成后触发中断）
     */
    ret = day34_program_dma(d, src_dma, DAY34_EDU_DEVBUF_OFFSET, len,
                            DAY34_EDU_DMA_CMD_START | DAY34_EDU_DMA_CMD_IRQ);
    if (ret)
        goto out;

    /*
     * Stage 2：EDU → RAM
     *   src = EDU 设备内存（固定偏移 0x40000）
     *   dst = DMA buffer 中的 dst 区域（虚拟地址）
     *   命令：START | DIR_TO_RAM | IRQ
     *   DIR_TO_RAM 位告诉 EDU 数据流向是"设备到 RAM"
     */
    ret = day34_program_dma(d, DAY34_EDU_DEVBUF_OFFSET, dst_dma, len,
                            DAY34_EDU_DMA_CMD_START | DAY34_EDU_DMA_CMD_DIR_TO_RAM | DAY34_EDU_DMA_CMD_IRQ);
    if (ret)
        goto out;

    // 计算 irq_delta（应该是 2，两段 DMA 各触发一次 IRQ）
    d->last_irq_delta = (u32)(d->irq_count - irq_before);
    d->last_run_ok = 1;
    d->total_run_ok++;

out:
    // 记录结束时间并计算耗时
    end_ns = ktime_get_ns();
    d->last_run_ns = end_ns - start_ns;
    d->last_run_error = ret;

    // 如果失败，累加失败计数
    if (ret)
        d->total_run_fail++;

    // 可选的 verbose 打印（成功时）
    if (unlikely(stability_verbose && !ret))
        dev_info(&d->pdev->dev, "run_dma ok: len=%u seed=0x%x irq_delta=%u",
                 len, seed, d->last_irq_delta);

    mutex_unlock(&d->op_lock);
    return ret;
}

/*
 * ==================== 第8部分：file_operations 回调 ====================
 *
 * Day34 的 file_operations：
 *   - open/release：标准的设备打开/关闭
 *   - read：读取设备信息（字符串格式，可直接 cat /dev/day34_edu0）
 *   - unlocked_ioctl：主要的控制接口
 *   - mmap：映射 DMA buffer 到用户态
 *   - llseek：禁止 llseek（设备不支持）
 *
 * 【与 Day33 的区别】
 *
 * Day33 的 read() 返回格式化的字符串用于 trace 分析。
 * Day34 的 read() 保持相同格式，但注释更强调"状态快照"用途。
 */
static int day34_open(struct inode *inode, struct file *filp)
{
    struct day34_dev *d = container_of(inode->i_cdev, struct day34_dev, cdev);
    filp->private_data = d;
    return 0;
}

static int day34_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/*
 * read() - 读取设备状态快照
 *
 * 格式化为字符串，可直接 cat /dev/day34_edu0 查看状态。
 * 包含：PCI ID、IRQ 统计、DMA 信息、运行统计、mmap 结果。
 */
static ssize_t day34_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    struct day34_dev *d = filp->private_data;
    char tmp[512];
    int n;

    n = scnprintf(tmp, sizeof(tmp),
                  "vendor=0x%04x"
                  "device=0x%04x"
                  "irq_count=%llu"
                  "last_irq_status=0x%08x"
                  "dma_handle=0x%llx"
                  "dma_bytes=%zu"
                  "dma_mask_bits=%u"
                  "total_run_calls=%llu"
                  "total_run_ok=%llu"
                  "total_run_fail=%llu"
                  "last_run_len=%u"
                  "last_run_seed=0x%x"
                  "last_run_ok=%u"
                  "last_run_error=%d"
                  "last_irq_delta=%u"
                  "last_mmap_ok=%u"
                  "last_mmap_error=%d"
                  "last_mmap_len=%u"
                  "last_mmap_pgoff=%u",
                  d->pdev->vendor, d->pdev->device, d->irq_count,
                  d->last_irq_status, (unsigned long long)d->dma_handle,
                  d->dma_bytes, d->dma_mask_bits, d->total_run_calls,
                  d->total_run_ok, d->total_run_fail, d->last_run_len,
                  d->last_run_seed, d->last_run_ok, d->last_run_error,
                  d->last_irq_delta, d->last_mmap_ok, d->last_mmap_error,
                  d->last_mmap_len, d->last_mmap_pgoff);
    return simple_read_from_buffer(buf, len, ppos, tmp, n);
}

/*
 * ioctl() - 设备控制接口
 *
 * Day34 支持的 ioctl 命令：
 *   DAY34_IOC_GET_INFO：获取设备信息（结构化）
 *   DAY34_IOC_RUN_DMA：执行一次 DMA 往返
 *   DAY34_IOC_GET_RESULT：获取最近一次运行结果
 *   DAY34_IOC_RESET_STATS：重置统计计数器
 *
 * 【与 Day33 的区别】
 *
 * Day34 的 ioctl 逻辑与 Day33 完全相同。
 * 区别在于注释更强调"错误注入"的边界检查。
 */
static long day34_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct day34_dev *d = filp->private_data;

    switch (cmd) {
    case DAY34_IOC_GET_INFO: {
        // 获取完整设备信息（用于用户态工具初始化）
        struct day34_info info = {0};
        info.tool_api_version = DAY34_TOOL_API_VERSION;
        info.vendor_id = d->pdev->vendor;
        info.device_id = d->pdev->device;
        info.irq_vector = d->irq_vector;
        info.irq_count = d->irq_count;
        info.last_irq_status = d->last_irq_status;
        info.last_ack_value = d->last_ack_value;
        info.bar0_start = d->bar0_start;
        info.bar0_len = d->bar0_len;
        info.dma_handle = d->dma_handle;
        info.dma_bytes = d->dma_bytes;
        info.dma_mask_bits = d->dma_mask_bits;
        info.msi_enabled = 1;
        info.map_bytes = PAGE_ALIGN(d->dma_bytes);
        info.src_off = DAY34_DMA_SRC_OFF;
        info.dst_off = DAY34_DMA_DST_OFF;
        info.max_verify_len = DAY34_DMA_VERIFY_MAX;
        info.total_run_calls = d->total_run_calls;
        info.total_run_ok = d->total_run_ok;
        info.total_run_fail = d->total_run_fail;
        info.last_run_ns = d->last_run_ns;
        info.last_run_len = d->last_run_len;
        info.last_run_seed = d->last_run_seed;
        info.last_run_ok = d->last_run_ok;
        info.last_run_error = d->last_run_error;
        info.last_irq_delta = d->last_irq_delta;
        info.last_dma_cmd = d->last_dma_cmd;
        info.last_mmap_ok = d->last_mmap_ok;
        info.last_mmap_error = d->last_mmap_error;
        info.last_mmap_len = d->last_mmap_len;
        info.last_mmap_pgoff = d->last_mmap_pgoff;
        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;
    }
    case DAY34_IOC_RUN_DMA: {
        // 执行一次 DMA 往返（传入 len 和 pattern_seed）
        struct day34_run_req req;
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;
        return day34_do_run_dma(d, req.len, req.pattern_seed);
    }
    case DAY34_IOC_GET_RESULT: {
        // 获取最近一次运行结果（结构化）
        struct day34_run_result res = {0};
        res.total_run_calls = d->total_run_calls;
        res.total_run_ok = d->total_run_ok;
        res.total_run_fail = d->total_run_fail;
        res.last_run_ns = d->last_run_ns;
        res.run_len = d->last_run_len;
        res.run_seed = d->last_run_seed;
        res.run_ok = d->last_run_ok;
        res.run_error = d->last_run_error;
        res.irq_delta = d->last_irq_delta;
        res.last_dma_cmd = d->last_dma_cmd;
        res.mmap_ok = d->last_mmap_ok;
        res.mmap_error = d->last_mmap_error;
        res.mmap_len = d->last_mmap_len;
        res.mmap_pgoff = d->last_mmap_pgoff;
        if (copy_to_user((void __user *)arg, &res, sizeof(res)))
            return -EFAULT;
        return 0;
    }
    case DAY34_IOC_RESET_STATS: {
        // 重置统计计数器（用于回归测试）
        mutex_lock(&d->op_lock);
        d->total_run_calls = 0;
        d->total_run_ok = 0;
        d->total_run_fail = 0;
        day34_reset_run_result(d);
        day34_record_mmap_result(d, false, 0, 0, 0);
        mutex_unlock(&d->op_lock);
        return 0;
    }
    default:
        return -ENOTTY;  // 不支持的 ioctl 命令
    }
}

/*
 * mmap() - 将 DMA buffer 映射到用户态
 *
 * 【错误注入：mmap offset 检查】
 *
 * Day34 只支持单页映射（vm_pgoff == 0）。
 * 如果用户传入非零 offset，说明他想映射 DMA buffer 以外的区域，
 * 这可能导致未定义行为，因此必须拒绝。
 *
 * 【为什么 len 必须等于 PAGE_ALIGN(dma_bytes)？】
 *
 *   - DMA buffer 大小是 4096 bytes
 *   - mmap 必须映射整个 buffer，不能多也不能少
 *   - 用户态通过 len 判断是否映射成功
 *
 * 【dma_mmap_coherent 的作用】
 *
 *   - 将 coherent DMA buffer 映射到用户态虚拟地址空间
 *   - 用户态直接访问这段内存，实际上是 DMA buffer
 *   - 实现了零拷贝：用户态和 EDU 设备共享同一块物理内存
 */
static int day34_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct day34_dev *d = filp->private_data;
    unsigned long len = vma->vm_end - vma->vm_start;

    // 【错误注入拒绝】pgoff 必须为 0
    if (vma->vm_pgoff != 0) {
        day34_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
        dev_err(&d->pdev->dev, "mmap rejected: pgoff=%lu must be 0", vma->vm_pgoff);
        return -EINVAL;
    }

    // 【错误注入拒绝】len 必须匹配 DMA buffer 大小
    if (len != PAGE_ALIGN(d->dma_bytes)) {
        day34_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
        dev_err(&d->pdev->dev, "mmap rejected: len=%lu expected=%lu",
                len, PAGE_ALIGN(d->dma_bytes));
        return -EINVAL;
    }

    // 执行 mmap
    if (dma_mmap_coherent(&d->pdev->dev, vma, d->dma_virt, d->dma_handle, d->dma_bytes)) {
        day34_record_mmap_result(d, false, -EAGAIN, len, vma->vm_pgoff);
        return -EAGAIN;
    }

    // 记录成功
    day34_record_mmap_result(d, true, 0, len, vma->vm_pgoff);
    return 0;
}

static const struct file_operations day34_fops = {
    .owner = THIS_MODULE,
    .open = day34_open,
    .release = day34_release,
    .read = day34_read,
    .unlocked_ioctl = day34_ioctl,
    .mmap = day34_mmap,
    .llseek = no_llseek,
};

/*
 * ==================== 第9部分：PCI probe/remove ====================
 *
 * 【probe() 的资源申请顺序】
 *
 *   1. devm_kzalloc() → 分配 day34_dev
 *   2. pci_enable_device() → 使能 PCI 设备
 *   3. dma_set_mask_and_coherent() → 设置 DMA mask
 *   4. pcim_iomap_regions() → 映射 BAR0 MMIO
 *   5. dma_alloc_coherent() → 分配 DMA buffer
 *   6. pci_alloc_irq_vectors() → 申请 MSI/LEGACY IRQ 向量
 *   7. request_irq() → 注册 IRQ handler
 *   8. cdev_add() → 注册字符设备
 *   9. device_create() → 创建设备节点
 *
 * 【remove() 的资源释放顺序（必须相反）】
 *
 *   1. device_destroy() → 销毁设备节点
 *   2. cdev_del() → 删除字符设备
 *   3. free_irq() → 释放 IRQ handler
 *   4. pci_free_irq_vectors() → 释放 IRQ 向量
 *   5. dma_free_coherent() → 释放 DMA buffer
 *   6. pcim_iounmap_regions() → （自动）
 *   7. pci_disable_device() → （自动）
 *
 * 【为什么使用 request_irq() 而不是 devm_request_irq()？】
 *
 *   Day34 要做 1000 次 insmod/rmmod 循环。
 *
 *   devm_request_irq() 的问题：
 *     - 由 devres 管理，释放时机在 remove() 返回之后
 *     - 若 remove() 中先 pci_free_irq_vectors()，MSI 仍被 IRQ 层引用
 *     - 可能触发 BUG in free_msi_irqs()
 *
 *   request_irq() 的好处：
 *     - 手动配对，释放时机完全可控
 *     - remove() 中先 free_irq() 再 pci_free_irq_vectors()
 *
 * 【双标志 irq_vectors_allocated 和 irq_requested】
 *
 *   - irq_vectors_allocated：MSI/LEGACY 向量是否已申请
 *   - irq_requested：IRQ handler 是否已注册
 *
 *   为什么要两个？
 *     - pci_alloc_irq_vectors() 成功 → irq_vectors_allocated = true
 *     - request_irq() 成功 → irq_requested = true
 *     - request_irq() 失败 → 需要回滚 pci_alloc_irq_vectors()
 *     - remove() 被调用但 probe() 部分失败 → 只释放已申请的资源
 */
static int day34_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day34_dev *d;
    int ret, minor;

    dev_info(&pdev->dev, "probe enter: %04x:%04x", pdev->vendor, pdev->device);

    // 1. 分配 day34_dev
    d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;
    pci_set_drvdata(pdev, d);
    d->pdev = pdev;

    // 初始化锁
    mutex_init(&d->op_lock);
    spin_lock_init(&d->irq_lock);

    // 设置 DMA 参数（可由模块参数覆盖）
    d->dma_bytes = DAY34_DMA_BYTES;
    d->dma_mask_bits = dma_mask_bits;

    // 2. 使能 PCI 设备
    ret = pcim_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "pcim_enable_device failed: %d", ret);
        return ret;
    }
    pci_set_master(pdev);  // 设置为 bus master（才能做 DMA）

    // 3. 设置 DMA mask
    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(d->dma_mask_bits));
    if (ret) {
        dev_err(&pdev->dev, "dma_set_mask_and_coherent(%u bits) failed: %d",
                d->dma_mask_bits, ret);
        return ret;
    }
    dev_info(&pdev->dev, "dma mask set to %u bits", d->dma_mask_bits);

    // 4. 映射 BAR0 MMIO
    ret = pcim_iomap_regions(pdev, BIT(0), DAY34_DRV_NAME);
    if (ret) {
        dev_err(&pdev->dev, "pcim_iomap_regions failed: %d", ret);
        return ret;
    }
    d->bar0 = pcim_iomap_table(pdev)[0];
    d->bar0_start = pci_resource_start(pdev, 0);
    d->bar0_len = pci_resource_len(pdev, 0);

    // 5. 分配 DMA coherent buffer
    d->dma_virt = dma_alloc_coherent(&pdev->dev, d->dma_bytes, &d->dma_handle, GFP_KERNEL);
    if (!d->dma_virt) {
        dev_err(&pdev->dev, "dma_alloc_coherent failed");
        return -ENOMEM;
    }
    dev_info(&pdev->dev,
             "dma_alloc_coherent ok: dma_handle=0x%llx dma_bytes=%zu dma_mask_bits=%u",
             (unsigned long long)d->dma_handle, d->dma_bytes, d->dma_mask_bits);

    // 6. 申请 IRQ 向量（MSI 优先，回退到 LEGACY）
    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
    if (ret < 0) {
        dev_err(&pdev->dev, "pci_alloc_irq_vectors failed: %d\n", ret);
        goto err_dma;
    }
    d->irq_vectors_allocated = true;  // 标记 MSI vectors 已申请
    d->irq_vector = pci_irq_vector(pdev, 0);

    // 7. 注册 IRQ handler（使用 request_irq 而不是 devm_request_irq）
    ret = request_irq(d->irq_vector, day34_irq_handler, 0, DAY34_DRV_NAME, d);
    if (ret) {
        dev_err(&pdev->dev, "request_irq failed: %d\n", ret);
        goto err_irq_vectors;
    }
    d->irq_requested = true;  // 标记 IRQ handler 已注册

    // 8. 注册字符设备
    minor = atomic_fetch_add_unless(&g_day34_minor, 1, INT_MAX);
    d->devt = MKDEV(MAJOR(g_day34_base_dev), minor);
    cdev_init(&d->cdev, &day34_fops);
    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret)
        goto err_irq_vectors;

    // 9. 创建设备节点（/dev/day34_edu0, /dev/day34_edu1, ...）
    d->device = device_create(g_day34_class, &pdev->dev, d->devt, d,
                              DAY34_DEV_NAME_FMT, minor);
    if (IS_ERR(d->device)) {
        ret = PTR_ERR(d->device);
        cdev_del(&d->cdev);
        goto err_irq_vectors;
    }

    dev_info(&pdev->dev,
             "probe success: dev=/dev/%s dma_handle=0x%llx bar0=[0x%pa + 0x%pa)",
             dev_name(d->device), (unsigned long long)d->dma_handle,
             &d->bar0_start, &d->bar0_len);
    return 0;

err_irq_vectors:
    // 回滚：释放 IRQ handler 和 MSI vectors
    if (d->irq_requested) {
        free_irq(d->irq_vector, d);
        d->irq_requested = false;
    }
    if (d->irq_vectors_allocated) {
        pci_free_irq_vectors(pdev);
        d->irq_vectors_allocated = false;
    }
err_dma:
    // 回滚：释放 DMA buffer
    dma_free_coherent(&pdev->dev, d->dma_bytes, d->dma_virt, d->dma_handle);
    return ret;
}

static void day34_remove(struct pci_dev *pdev)
{
    struct day34_dev *d = pci_get_drvdata(pdev);

    if (!d)
        return;

    // 1. 销毁设备节点
    if (d->device)
        device_destroy(g_day34_class, d->devt);

    // 2. 删除字符设备
    cdev_del(&d->cdev);

    /*
     * 3-5. 释放资源的顺序（与申请顺序相反）：
     *
     *   【关键】先 free_irq，再 pci_free_irq_vectors
     *
     *   如果顺序反过来：
     *     - pci_free_irq_vectors() 释放 MSI vector
     *     - 但 IRQ handler 仍然注册在该 vector 上
     *     - 内核 IRQ layer 可能触发 BUG
     *
     *   Day34 的 1000 次模块循环就是验证这个顺序是否稳定。
     */
    if (d->irq_requested) {
        free_irq(d->irq_vector, d);
        d->irq_requested = false;
    }
    if (d->irq_vectors_allocated) {
        pci_free_irq_vectors(pdev);
        d->irq_vectors_allocated = false;
    }

    // 6. 释放 DMA buffer
    if (d->dma_virt)
        dma_free_coherent(&pdev->dev, d->dma_bytes, d->dma_virt, d->dma_handle);

    dev_info(&pdev->dev, "remove complete");
}

/*
 * ==================== 第10部分：模块初始化/退出 ====================
 *
 * 【init() 的初始化顺序】
 *
 *   1. alloc_chrdev_region() → 申请字符设备号范围
 *   2. class_create() → 创建 sysfs 类
 *   3. pci_register_driver() → 注册 PCI 驱动
 *
 * 【exit() 的退出顺序（与 init 相反）】
 *
 *   1. pci_unregister_driver() → 注销 PCI 驱动（触发 remove）
 *   2. class_destroy() → 销毁 sysfs 类
 *   3. unregister_chrdev_region() → 释放字符设备号范围
 */
static const struct pci_device_id day34_ids[] = {
    { PCI_DEVICE(DAY34_EDU_VENDOR_ID, DAY34_EDU_DEVICE_ID) },
    { }
};
MODULE_DEVICE_TABLE(pci, day34_ids);

static struct pci_driver day34_pci_driver = {
    .name = DAY34_DRV_NAME,
    .id_table = day34_ids,
    .probe = day34_probe,
    .remove = day34_remove,
};

static int __init day34_init(void)
{
    int ret;

    // 1. 申请字符设备号（最多 256 个次设备号，支持多实例）
    ret = alloc_chrdev_region(&g_day34_base_dev, 0, 256, DAY34_DRV_NAME);
    if (ret)
        return ret;

    // 2. 创建 sysfs 类（/sys/class/day34_edu/）
    g_day34_class = class_create(THIS_MODULE, DAY34_CLASS_NAME);
    if (IS_ERR(g_day34_class)) {
        unregister_chrdev_region(g_day34_base_dev, 256);
        return PTR_ERR(g_day34_class);
    }

    // 3. 注册 PCI 驱动（触发 probe）
    ret = pci_register_driver(&day34_pci_driver);
    if (ret) {
        class_destroy(g_day34_class);
        unregister_chrdev_region(g_day34_base_dev, 256);
        return ret;
    }

    pr_info("%s: init ok", DAY34_DRV_NAME);
    return 0;
}

static void __exit day34_exit(void)
{
    // 1. 注销 PCI 驱动（触发所有设备的 remove）
    pci_unregister_driver(&day34_pci_driver);

    // 2. 销毁 sysfs 类
    class_destroy(g_day34_class);

    // 3. 释放字符设备号
    unregister_chrdev_region(g_day34_base_dev, 256);

    pr_info("%s: exit ok", DAY34_DRV_NAME);
}

module_init(day34_init);
module_exit(day34_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Day34 stability EDU driver");

/*
 * ==================== 附录A：稳定性测试三大场景 ====================
 *
 * 【场景1：并发压测】
 *
 *   目标：验证多进程并发访问同一设备时不会误报失败
 *
 *   工具：stress-mmap x3 + stress-ioctl x1
 *   协调：flock() 保护用户态共享缓冲区
 *   验证：所有 worker rc=0，worker_fail=0
 *
 * 【场景2：模块循环】
 *
 *   目标：验证 1000 次 insmod/rmmod 不会导致资源泄漏
 *
 *   命令：for i in 1..1000; do rmmod; insmod; done
 *   验证：completed_loops=1000，failed_loops=0
 *
 *   关键：
 *     - free_irq() → pci_free_irq_vectors() 顺序不能错
 *     - 两个标志 irq_vectors_allocated/irq_requested 确保正确回滚
 *
 * 【场景3：错误注入】
 *
 *   目标：验证非法输入被明确拒绝
 *
 *   测试1：len > DAY34_DMA_VERIFY_MAX → -EINVAL
 *   测试2：vm_pgoff != 0 → -EINVAL
 *
 *   验证：mmap_ok=0，mmap_error=负数 errno
 */

/*
 * ==================== 附录B：用户态工具如何验证稳定性 ====================
 *
 * 【stress-mmap 验证流程】
 *
 *   1. flock(LOCK_EX) → 获取排他锁
 *   2. mmap() → 映射 DMA buffer
 *   3. memset(src, seed, len) → 填充测试数据
 *   4. ioctl(RUN_DMA) → 触发 DMA
 *   5. memcmp(src, dst, len) → 验证数据完整性
 *   6. munmap() → 解除映射
 *   7. flock(LOCK_UN) → 释放锁
 *
 * 【stress-ioctl 验证流程】
 *
 *   1. ioctl(RUN_DMA) → 触发 DMA
 *   2. 检查返回值是否为 0
 *
 * 【模块循环验证】
 *
 *   1. insmod day34_edu_stability.ko
 *   2. 检查 /dev/day34_edu* 是否存在
 *   3. rmmod day34_edu_stability
 *   4. 检查模块是否完全卸载（/proc/modules 无残留）
 *   5. 重复 1000 次
 */

/*
 * ==================== 附录C：Day34 与 Day33 的代码对比 ====================
 *
 * 【共同点】
 *
 *   - DMA buffer 布局（src @ 0, dst @ 2048, 4096 bytes 总大小）
 *   - 两段 DMA（RAM→EDU, EDU→RAM）
 *   - mmap 使用 dma_mmap_coherent()
 *   - ioctl 接口（GET_INFO/RUN_DMA/GET_RESULT/RESET_STATS）
 *   - ktime_get_ns() 计时
 *   - op_lock 互斥锁保护 device state
 *
 * 【差异点】
 *
 *   Day33：
 *     - 模块参数：trace_verbose（用于 ftrace 调试）
 *     - 使用 devm_request_irq()（简化代码）
 *     - 无双标志（因为 devm 自动管理）
 *
 *   Day34：
 *     - 模块参数：stability_verbose（用于稳定性测试）
 *     - 使用 request_irq()/free_irq() 手动配对
 *     - 新增 irq_vectors_allocated 和 irq_requested 双标志
 *     - mmap 拒绝非零 pgoff（错误注入）
 *     - 更详细的错误处理和回滚逻辑
 */
