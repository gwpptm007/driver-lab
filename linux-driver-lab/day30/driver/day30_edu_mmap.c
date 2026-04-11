// SPDX-License-Identifier: GPL-2.0
/*
 * Day30 - QEMU EDU coherent DMA + mmap 零拷贝学习驱动
 *
 * ==================== 驱动目标 ====================
 *
 * Day30 承接 Day29 的 coherent DMA 基础，引入 mmap 零拷贝访问。
 *
 * 【Day29 vs Day30 的核心区别】
 *   Day29：内核是主角（内核 fill_pattern、memset、compare）
 *   Day30：用户态是主角（用户态 mmap 后直接 fill/clear/compare）
 *
 * 【Day30 的职责划分】
 *   用户态负责：通过 mmap 直接读写 coherent DMA buffer
 *   内核负责：dma_alloc_coherent()、MMIO、DMA 编程、IRQ 处理、边界守门
 *
 * ==================== 头文件依赖 ====================
 *
 * DMA 和 mmap 相关：
 *   linux/dma-mapping.h → dma_set_mask_and_coherent、dma_alloc_coherent
 *   linux/mm.h          → dma_mmap_coherent、vm_area_struct、PAGE_ALIGN
 *
 * 字符设备相关：
 *   linux/cdev.h        → struct cdev
 *   linux/device.h      → struct class, struct device
 *   linux/fs.h          → struct file_operations
 *
 * PCI 相关：
 *   linux/pci.h         → struct pci_dev、pci_iomap 等
 *
 * 其他：
 *   linux/mutex.h       → struct mutex（保护 DMA 操作）
 *   linux/spinlock.h    → struct spinlock（保护中断共享数据）
 *   linux/uaccess.h      → copy_to_user、copy_from_user
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
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "include/day30_edu_mmap.h"
#include "../include/day30_edu_uapi.h"

/*
 * ==================== 模块全局变量 ====================
 *
 * g_day30_base_dev：分配到的设备号范围（主设备号+次设备号基数）
 * g_day30_class：sysfs class 指针
 * g_day30_minor：原子计数器，为每个设备分配唯一次设备号
 * dma_mask_bits：可加载参数，默认 32（Day30 自动化放宽）
 */
static dev_t g_day30_base_dev;
static struct class *g_day30_class;
static atomic_t g_day30_minor = ATOMIC_INIT(0);

static unsigned int dma_mask_bits = DAY30_DMA_MASK_BITS_DEFAULT;
module_param(dma_mask_bits, uint, 0644);
MODULE_PARM_DESC(dma_mask_bits,
		 "DMA mask bits for QEMU EDU (default 32 in day30 automation)");

/*
 * ==================== 寄存器读写辅助函数 ====================
 *
 * day30_read32：读 32-bit 寄存器
 * day30_write32：写 32-bit 寄存器
 * day30_write64：写 64-bit 寄存器（DMA 地址需要 64-bit）
 *
 * 注意：DMA_SRC 和 DMA_DST 是 64-bit 寄存器，必须用 writeq
 */
static inline u32 day30_read32(struct day30_dev *d, u32 off)
{
	return readl(d->bar0 + off);
}

static inline void day30_write32(struct day30_dev *d, u32 off, u32 val)
{
	writel(val, d->bar0 + off);
}

static inline void day30_write64(struct day30_dev *d, u32 off, u64 val)
{
	writeq(val, d->bar0 + off);
}

/*
 * ==================== 中断处理函数 ====================
 *
 * 与 Day29 相同：
 *   1. 读 IRQ_STATUS 判断是否有中断
 *   2. 自旋锁保护共享数据（irq_count、last_irq_status、last_ack_value）
 *   3. 写 IRQ_ACK 清除中断
 */
static irqreturn_t day30_irq_handler(int irq, void *opaque)
{
	struct day30_dev *d = opaque;
	unsigned long flags;
	u32 status;

	/*
	 * 读取 IRQ_STATUS，判断是否是 EDU 设备的中断
	 * 如果 status == 0，说明不是我们的中断，返回 IRQ_NONE
	 */
	status = day30_read32(d, DAY30_EDU_REG_IRQ_STATUS);
	if (!status)
		return IRQ_NONE;

	/*
	 * 自旋锁保护共享数据：
	 *   irq_count：中断计数
	 *   last_irq_status：最近一次中断状态
	 *   last_ack_value：最近一次清除值
	 */
	spin_lock_irqsave(&d->irq_lock, flags);
	d->irq_count++;
	d->last_irq_status = status;
	d->last_ack_value = status;
	spin_unlock_irqrestore(&d->irq_lock, flags);

	/*
	 * 写 IRQ_ACK 清除中断
	 * 注意：写入的值等于读出的 status
	 */
	day30_write32(d, DAY30_EDU_REG_IRQ_ACK, status);

	dev_info(&d->pdev->dev,
		 "irq handler: irq=%d status=0x%08x count=%llu\n",
		 irq, status, d->irq_count);
	return IRQ_HANDLED;
}

/*
 * ==================== DMA 等待与编程 ====================
 *
 * day30_wait_dma_idle：轮询等待 DMA 完成
 * day30_program_dma：配置 DMA 寄存器并触发传输
 *
 * 【DMA 完成的判断】
 *   读 DMA_CMD，如果 START 位（bit0）为 0，说明 DMA 已完成
 *   START 位从 1 变回 0 表示一次 DMA 传输结束
 */
static int day30_wait_dma_idle(struct day30_dev *d)
{
	int i;
	u32 cmd;

	/*
	 * 最多等待 50000 * 10us = 500ms
	 * 如果超时说明 DMA 出了问题
	 */
	for (i = 0; i < 50000; ++i) {
		cmd = day30_read32(d, DAY30_EDU_REG_DMA_CMD);
		if (!(cmd & DAY30_EDU_DMA_CMD_START))
			return 0;
		udelay(10);
	}

	dev_err(&d->pdev->dev, "DMA timeout: cmd=0x%08x\n",
		day30_read32(d, DAY30_EDU_REG_DMA_CMD));
	return -ETIMEDOUT;
}

static int day30_program_dma(struct day30_dev *d, u64 src, u64 dst,
			     u32 count, u32 cmd)
{
	if (!count)
		return -EINVAL;

	/*
	 * 记录本次 DMA 命令（用于 records 和调试）
	 */
	d->last_dma_cmd = cmd;

	/*
	 * 配置 DMA 寄存器顺序：
	 *   1. DMA_SRC（64-bit）：源地址
	 *   2. DMA_DST（64-bit）：目的地址
	 *   3. DMA_COUNT（32-bit）：传输字节数
	 *   4. DMA_CMD（32-bit）：命令（写入时触发 DMA）
	 *
	 * 注意：先配置寄存器，最后写 CMD 才触发传输
	 */
	day30_write64(d, DAY30_EDU_REG_DMA_SRC, src);
	day30_write64(d, DAY30_EDU_REG_DMA_DST, dst);
	day30_write32(d, DAY30_EDU_REG_DMA_COUNT, count);
	day30_write32(d, DAY30_EDU_REG_DMA_CMD, cmd);

	return day30_wait_dma_idle(d);
}

/*
 * ==================== 状态记录函数 ====================
 *
 * day30_reset_run_result：重置 DMA 运行结果
 * day30_record_mmap_result：记录 mmap 结果
 *
 * 【为什么需要单独记录 mmap 结果？】
 *   mmap 可能因为多种原因失败：pgoff != 0、len != map_bytes、dma_mmap_coherent 内部错误
 *   这些信息需要返回给用户态用于调试
 */
static void day30_reset_run_result(struct day30_dev *d)
{
	d->last_run_len = 0;
	d->last_run_seed = 0;
	d->last_run_error = 0;
	d->last_run_ok = 0;
	d->last_irq_delta = 0;
	d->last_dma_cmd = 0;
}

static void day30_record_mmap_result(struct day30_dev *d, bool ok,
				     int err, unsigned long len,
				     unsigned long pgoff)
{
	d->last_mmap_ok = ok ? 1U : 0U;
	d->last_mmap_error = err;
	d->last_mmap_len = (u32)len;
	d->last_mmap_pgoff = (u32)pgoff;
}

/*
 * ==================== 核心：DMA 运行函数 ====================
 *
 * day30_do_run_dma：执行两段 DMA 往返
 *
 * 【与 Day29 day29_do_verify 的区别】
 *   Day29：内核负责 fill_pattern、memset、memcmp
 *   Day30：内核只负责发起 DMA，不碰 buffer 内容
 *          用户态通过 mmap 直接访问 buffer
 *
 * 【DMA 往返流程】
 *   1. RAM(src) → EDU(0x40000)
 *   2. EDU(0x40000) → RAM(dst)
 *
 * 【参数】
 *   len：验证长度（<= 2048）
 *   seed：pattern 种子（Day30 不使用，保留兼容性）
 */
static int day30_do_run_dma(struct day30_dev *d, u32 len, u32 seed)
{
	u64 src_dma;
	u64 dst_dma;
	u64 irq_before;
	int ret;

	/*
	 * 合法性检查
	 * 注意：Day30 不检查 d->dma_virt 是否为 NULL
	 * 因为 Day30 的主角是用户态，内核不访问 buffer
	 */
	if (!d->dma_virt)
		return -ENODEV;
	if (!len || len > DAY30_DMA_VERIFY_MAX)
		return -EINVAL;

	mutex_lock(&d->op_lock);

	/*
	 * 重置运行结果并记录参数
	 */
	day30_reset_run_result(d);
	d->last_run_len = len;
	d->last_run_seed = seed;

	/*
	 * 计算 DMA 地址：
	 *   src_dma = dma_handle + 0
	 *   dst_dma = dma_handle + 2048
	 *
	 * 注意：这里用的是 dma_handle（设备地址），不是 dma_virt（虚拟地址）
	 */
	src_dma = (u64)d->dma_handle + DAY30_DMA_SRC_OFF;
	dst_dma = (u64)d->dma_handle + DAY30_DMA_DST_OFF;

	/*
	 * 记录 DMA 开始前的中断计数
	 * 用于计算 irq_delta（应该是 2，两段 DMA 各触发一次）
	 */
	irq_before = d->irq_count;

	dev_info(&d->pdev->dev,
		 "run_dma start: len=%u seed=0x%x src_dma=0x%llx dst_dma=0x%llx\n",
		 len, seed,
		 (unsigned long long)src_dma,
		 (unsigned long long)dst_dma);

	/*
	 * 【第一次 DMA：RAM → EDU】
	 *   src = RAM 的 coherent buffer
	 *   dst = EDU 内部 buffer（固定偏移 0x40000）
	 *   cmd = START | IRQ = 0x01 | 0x04 = 0x05
	 */
	ret = day30_program_dma(d, src_dma, DAY30_EDU_DEVBUF_OFFSET,
				len,
				DAY30_EDU_DMA_CMD_START |
				DAY30_EDU_DMA_CMD_IRQ);
	if (ret) {
		d->last_run_error = ret;
		dev_err(&d->pdev->dev, "run_dma stage1 RAM->EDU failed: %d\n", ret);
		goto out;
	}

	/*
	 * 【第二次 DMA：EDU → RAM】
	 *   src = EDU 内部 buffer（固定偏移 0x40000）
	 *   dst = RAM 的 coherent buffer（dst 区，偏移 2048）
	 *   cmd = START | DIR | IRQ = 0x01 | 0x02 | 0x04 = 0x07
	 */
	ret = day30_program_dma(d, DAY30_EDU_DEVBUF_OFFSET, dst_dma,
				len,
				DAY30_EDU_DMA_CMD_START |
				DAY30_EDU_DMA_CMD_DIR_TO_RAM |
				DAY30_EDU_DMA_CMD_IRQ);
	if (ret) {
		d->last_run_error = ret;
		dev_err(&d->pdev->dev, "run_dma stage2 EDU->RAM failed: %d\n", ret);
		goto out;
	}

	/*
	 * DMA 完成，记录结果
	 * 注意：Day30 不做 memcmp，比较留给用户态
	 */
	d->last_irq_delta = (u32)(d->irq_count - irq_before);
	d->last_run_ok = 1;
	dev_info(&d->pdev->dev,
		 "run_dma ok: len=%u seed=0x%x irq_delta=%u\n",
		 len, seed, d->last_irq_delta);

out:
	mutex_unlock(&d->op_lock);
	return d->last_run_error;
}

/*
 * ==================== 状态文本构建 ====================
 *
 * day30_build_state_text：生成可读状态字符串
 * 用于 cat /dev/day30_edu0 读取设备状态
 */
static ssize_t day30_build_state_text(struct day30_dev *d, char *buf, size_t size)
{
	return scnprintf(buf, size,
			 "vendor=0x%04x device=0x%04x\n"
			 "bar0_start=0x%llx bar0_len=0x%llx\n"
			 "irq_vector=%u irq_count=%llu msi_enabled=%u\n"
			 "last_irq_status=0x%08x last_ack_value=0x%08x\n"
			 "dma_handle=0x%llx dma_bytes=%zu dma_mask_bits=%u\n"
			 "map_bytes=%lu src_off=%u dst_off=%u max_verify_len=%u\n"
			 "run_len=%u run_seed=0x%x run_ok=%u run_error=%d\n"
			 "last_irq_delta=%u last_dma_cmd=0x%08x\n"
			 "mmap_ok=%u mmap_error=%d mmap_len=%u mmap_pgoff=%u\n",
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
			 PAGE_ALIGN(d->dma_bytes),
			 DAY30_DMA_SRC_OFF,
			 DAY30_DMA_DST_OFF,
			 DAY30_DMA_VERIFY_MAX,
			 d->last_run_len,
			 d->last_run_seed,
			 d->last_run_ok,
			 d->last_run_error,
			 d->last_irq_delta,
			 d->last_dma_cmd,
			 d->last_mmap_ok,
			 d->last_mmap_error,
			 d->last_mmap_len,
			 d->last_mmap_pgoff);
}

/*
 * ==================== 文件操作集合 ====================
 *
 * day30_open：打开设备，获取私有数据
 * day30_read：读取设备状态文本
 * day30_mmap：【核心新增】将 DMA buffer 映射到用户态
 * day30_ioctl：设备控制命令
 */
static int day30_open(struct inode *inode, struct file *file)
{
	struct day30_dev *d = container_of(inode->i_cdev, struct day30_dev, cdev);

	file->private_data = d;
	return 0;
}

static ssize_t day30_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct day30_dev *d = file->private_data;
	char kbuf[384];
	ssize_t len;

	if (!d || !d->bar0)
		return -ENODEV;

	len = day30_build_state_text(d, kbuf, sizeof(kbuf));
	return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

/*
 * ==================== 【核心新增】mmap 实现 ====================
 *
 * day30_mmap：将 coherent DMA buffer 映射到用户态
 *
 * 【mmap 语义】
 *   用户态调用 mmap(fd, offset=0) 后，会建立从用户虚拟地址到 DMA buffer 的页表映射
 *   之后用户态访问该虚拟地址就等于直接访问 coherent buffer
 *
 * 【边界校验】
 *   Day30 只允许"整页映射 + offset=0"：
 *     - vm_pgoff 必须为 0
 *     - len 必须等于 PAGE_ALIGN(dma_bytes)
 *
 * 【页对齐陷阱】
 *   mmap 的 length 参数在 VMA 建立时会按页向上取整
 *   例如在 4KB 页下请求 2048 字节，实际会分配 4096 字节
 *   因此 invalid-length 测试要用 4097 这类跨页长度
 */
static int day30_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct day30_dev *d = file->private_data;
	unsigned long len = vma->vm_end - vma->vm_start;
	unsigned long map_bytes = PAGE_ALIGN(d->dma_bytes);
	int ret;

	if (!d || !d->dma_virt)
		return -ENODEV;

	/*
	 * 边界校验 1：只允许 offset == 0
	 * 原因：Day30 重点是零拷贝访问，不是 VMA 切片
	 */
	if (vma->vm_pgoff != 0) {
		day30_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
		dev_err(&d->pdev->dev,
			"mmap rejected: pgoff=%lu expected=0\n",
			vma->vm_pgoff);
		return -EINVAL;
	}

	/*
	 * 边界校验 2：只允许 length == PAGE_ALIGN(dma_bytes)
	 * 原因：coherent buffer 当前就是 4KB，不允许半页或其他长度
	 */
	if (len != map_bytes) {
		day30_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
		dev_err(&d->pdev->dev,
			"mmap rejected: len=%lu expected=%lu\n",
			len, map_bytes);
		return -EINVAL;
	}

	/*
	 * 设置 VMA 标志：
	 *   VM_IO：该内存是 I/O 设备内存，不是普通 RAM
	 *   VM_DONTDUMP：从 core dump 中排除（I/O 映射不需要）
	 *   VM_DONTEXPAND：不允许 expand（保护 buffer 大小）
	 */
	vma->vm_flags |= VM_IO | VM_DONTDUMP | VM_DONTEXPAND;

	/*
	 * 【关键调用】dma_mmap_coherent
	 *
	 * 这个函数负责：
	 *   1. 建立用户虚拟地址到 DMA buffer 物理地址的页表映射
	 *   2. 确保 cache 一致性（coherent 的含义）
	 *
	 * 参数：
	 *   &d->pdev->dev：设备上下文
	 *   vma：VMA（已设置好 flags）
	 *   d->dma_virt：CPU 访问用的虚拟地址
	 *   d->dma_handle：设备 DMA 地址
	 *   d->dma_bytes：buffer 大小
	 *
	 * 返回值：0=成功，负值=错误码
	 */
	ret = dma_mmap_coherent(&d->pdev->dev, vma,
				d->dma_virt, d->dma_handle, d->dma_bytes);

	/*
	 * 记录 mmap 结果到 day30_dev，供 GET_INFO/GET_RESULT 返回
	 */
	day30_record_mmap_result(d, ret == 0, ret, len, vma->vm_pgoff);

	if (ret)
		dev_err(&d->pdev->dev, "dma_mmap_coherent failed: %d\n", ret);
	else
		dev_info(&d->pdev->dev,
			 "mmap ok: len=%lu pgoff=%lu dma=0x%llx\n",
			 len, vma->vm_pgoff,
			 (unsigned long long)d->dma_handle);
	return ret;
}

/*
 * ==================== ioctl 命令处理 ====================
 *
 * DAY30_IOC_GET_INFO：获取完整设备信息
 * DAY30_IOC_RUN_DMA：触发两段 DMA 往返
 * DAY30_IOC_GET_RESULT：获取最近一次运行和 mmap 结果
 * DAY30_IOC_RESET_STATS：重置统计计数
 */
static long day30_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct day30_dev *d = file->private_data;

	switch (cmd) {
	case DAY30_IOC_GET_INFO: {
		struct day30_info info = {
			.tool_api_version = DAY30_TOOL_API_VERSION,
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
			.map_bytes = PAGE_ALIGN(d->dma_bytes),
			.src_off = DAY30_DMA_SRC_OFF,
			.dst_off = DAY30_DMA_DST_OFF,
			.max_verify_len = DAY30_DMA_VERIFY_MAX,
			.last_run_len = d->last_run_len,
			.last_run_seed = d->last_run_seed,
			.last_run_ok = d->last_run_ok,
			.last_run_error = d->last_run_error,
			.last_irq_delta = d->last_irq_delta,
			.last_dma_cmd = d->last_dma_cmd,
			.last_mmap_ok = d->last_mmap_ok,
			.last_mmap_error = d->last_mmap_error,
			.last_mmap_len = d->last_mmap_len,
			.last_mmap_pgoff = d->last_mmap_pgoff,
		};

		if (copy_to_user((void __user *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}
	case DAY30_IOC_RUN_DMA: {
		struct day30_run_req req;

		if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
			return -EFAULT;
		return day30_do_run_dma(d, req.len, req.pattern_seed);
	}
	case DAY30_IOC_GET_RESULT: {
		struct day30_run_result res = {
			.run_len = d->last_run_len,
			.run_seed = d->last_run_seed,
			.run_ok = d->last_run_ok,
			.run_error = d->last_run_error,
			.irq_delta = d->last_irq_delta,
			.last_dma_cmd = d->last_dma_cmd,
			.mmap_ok = d->last_mmap_ok,
			.mmap_error = d->last_mmap_error,
			.mmap_len = d->last_mmap_len,
			.mmap_pgoff = d->last_mmap_pgoff,
		};

		if (copy_to_user((void __user *)arg, &res, sizeof(res)))
			return -EFAULT;
		return 0;
	}
	case DAY30_IOC_RESET_STATS:
		mutex_lock(&d->op_lock);
		d->irq_count = 0;
		d->last_irq_status = 0;
		d->last_ack_value = 0;
		day30_reset_run_result(d);
		day30_record_mmap_result(d, false, 0, 0, 0);
		mutex_unlock(&d->op_lock);
		return 0;
	default:
		return -ENOTTY;
	}
}

/*
 * ==================== 文件操作集合定义 ====================
 */
static const struct file_operations day30_fops = {
	.owner          = THIS_MODULE,
	.open           = day30_open,
	.read           = day30_read,
	.mmap           = day30_mmap,
	.unlocked_ioctl = day30_ioctl,
	.llseek         = no_llseek,
};

/*
 * ==================== 字符设备注册/注销 ====================
 *
 * day30_setup_chrdev：创建设备节点
 * day30_destroy_chrdev：销毁设备节点
 */
static int day30_setup_chrdev(struct day30_dev *d)
{
	int minor;
	int ret;

	minor = atomic_fetch_add(1, &g_day30_minor);
	d->devt = MKDEV(MAJOR(g_day30_base_dev), minor);

	cdev_init(&d->cdev, &day30_fops);
	d->cdev.owner = THIS_MODULE;

	ret = cdev_add(&d->cdev, d->devt, 1);
	if (ret)
		return ret;

	/*
	 * device_create：
	 *   创建 /dev/day30_edu0 等设备节点
	 *   同时在 /sys/class/day30_edu/ 下创建链接
	 */
	d->device = device_create(g_day30_class, &d->pdev->dev, d->devt, NULL,
				  DAY30_DEV_NAME_FMT, minor);
	if (IS_ERR(d->device)) {
		ret = PTR_ERR(d->device);
		d->device = NULL;
		cdev_del(&d->cdev);
		return ret;
	}
	return 0;
}

static void day30_destroy_chrdev(struct day30_dev *d)
{
	if (d->device)
		device_destroy(g_day30_class, d->devt);
	cdev_del(&d->cdev);
}

/*
 * ==================== PCI probe/remove ====================
 *
 * 与 Day29 几乎完全相同：
 *   1. 分配 day30_dev
 *   2. 设置 DMA mask
 *   3. 启用设备、请求 BAR、映射 BAR0
 *   4. 分配 coherent DMA buffer
 *   5. 注册中断
 *   6. 注册字符设备
 *
 * 唯一区别：probe 末尾调用 day30_record_mmap_result 初始化 mmap 状态
 */
static int day30_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct day30_dev *d;
	u32 ident;
	u32 live;
	int ret;

	dev_info(&pdev->dev, "probe enter: %04x:%04x\n", pdev->vendor, pdev->device);

	/*
	 * 分配 day30_dev 结构体
	 */
	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	d->pdev = pdev;
	d->dma_mask_bits = dma_mask_bits;
	d->dma_bytes = DAY30_DMA_BYTES;
	spin_lock_init(&d->irq_lock);
	mutex_init(&d->op_lock);
	pci_set_drvdata(pdev, d);

	/*
	 * 启用 PCI 设备
	 */
	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "pci_enable_device failed: %d\n", ret);
		goto err_free;
	}

	/*
	 * 设置 DMA mask
	 * Day30 自动化使用 -device edu,dma_mask=0xffffffff 放宽到 32-bit
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

	/*
	 * 请求 PCI I/O 和 Memory 资源
	 */
	ret = pci_request_regions(pdev, DAY30_DRV_NAME);
	if (ret) {
		dev_err(&pdev->dev, "pci_request_regions failed: %d\n", ret);
		goto err_disable;
	}

	/*
	 * 设置为主设备（DMA 需要）
	 */
	pci_set_master(pdev);

	/*
	 * 获取 BAR0 信息并映射
	 */
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

	/*
	 * 读取 ID 和 LIVENESS 寄存器（用于调试信息）
	 */
	ident = day30_read32(d, DAY30_EDU_REG_IDENTITY);
	live = day30_read32(d, DAY30_EDU_REG_LIVENESS);
	dev_info(&pdev->dev, "ident=0x%08x liveness=0x%08x\n", ident, live);

	/*
	 * 【Day30 核心】分配 coherent DMA buffer
	 *
	 * 返回两个地址：
	 *   dma_virt：CPU 访问用的虚拟地址
	 *   dma_handle：设备访问用的 DMA 地址
	 *
	 * 注意：这个 buffer 会通过 mmap 暴露给用户态
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

	/*
	 * 分配 MSI 中断并注册 handler
	 */
	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
	if (ret < 0) {
		dev_err(&pdev->dev, "pci_alloc_irq_vectors failed: %d\n", ret);
		goto err_dma;
	}
	d->irq_vector = pci_irq_vector(pdev, 0);

	ret = request_irq(d->irq_vector, day30_irq_handler, 0, DAY30_DRV_NAME, d);
	if (ret) {
		dev_err(&pdev->dev, "request_irq(%u) failed: %d\n",
			d->irq_vector, ret);
		goto err_irq_vectors;
	}
	dev_info(&pdev->dev, "MSI vector=%u enabled=%u\n",
		 d->irq_vector, !!pdev->msi_enabled);

	/*
	 * 注册字符设备
	 */
	ret = day30_setup_chrdev(d);
	if (ret) {
		dev_err(&pdev->dev, "day30_setup_chrdev failed: %d\n", ret);
		goto err_irq;
	}

	/*
	 * 初始化运行结果和 mmap 结果
	 */
	day30_reset_run_result(d);
	day30_record_mmap_result(d, false, 0, 0, 0);
	dev_info(&pdev->dev, "probe success\n");
	return 0;

err_irq:
	free_irq(d->irq_vector, d);
err_irq_vectors:
	pci_free_irq_vectors(pdev);
err_dma:
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

static void day30_remove(struct pci_dev *pdev)
{
	struct day30_dev *d = pci_get_drvdata(pdev);

	if (!d)
		return;

	dev_info(&pdev->dev, "remove enter\n");
	day30_destroy_chrdev(d);
	free_irq(d->irq_vector, d);
	pci_free_irq_vectors(pdev);
	if (d->dma_virt)
		dma_free_coherent(&pdev->dev, d->dma_bytes,
				  d->dma_virt, d->dma_handle);
	if (d->bar0)
		pci_iounmap(pdev, d->bar0);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	dev_info(&pdev->dev, "remove leave\n");
	kfree(d);
}

/*
 * ==================== PCI ID 表和驱动注册 ====================
 */
static const struct pci_device_id day30_pci_ids[] = {
	{ PCI_DEVICE(DAY30_EDU_VENDOR_ID, DAY30_EDU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, day30_pci_ids);

static struct pci_driver day30_pci_driver = {
	.name = DAY30_DRV_NAME,
	.id_table = day30_pci_ids,
	.probe = day30_probe,
	.remove = day30_remove,
};

/*
 * ==================== 模块初始化/退出 ====================
 */
static int __init day30_init(void)
{
	int ret;

	/*
	 * 分配 256 个次设备号（足够支持多个 EDU 设备）
	 */
	ret = alloc_chrdev_region(&g_day30_base_dev, 0, 256, DAY30_DRV_NAME);
	if (ret)
		return ret;

	/*
	 * 创建 sysfs class
	 */
	g_day30_class = class_create(THIS_MODULE, DAY30_CLASS_NAME);
	if (IS_ERR(g_day30_class)) {
		ret = PTR_ERR(g_day30_class);
		unregister_chrdev_region(g_day30_base_dev, 256);
		return ret;
	}

	/*
	 * 注册 PCI 驱动
	 * 之后 QEMU EDU 设备被发现时，probe 会被调用
	 */
	ret = pci_register_driver(&day30_pci_driver);
	if (ret) {
		class_destroy(g_day30_class);
		unregister_chrdev_region(g_day30_base_dev, 256);
		return ret;
	}

	pr_info(DAY30_DRV_NAME ": init ok\n");
	return 0;
}

static void __exit day30_exit(void)
{
	pci_unregister_driver(&day30_pci_driver);
	class_destroy(g_day30_class);
	unregister_chrdev_region(g_day30_base_dev, 256);
	pr_info(DAY30_DRV_NAME ": exit ok\n");
}

module_init(day30_init);
module_exit(day30_exit);

MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Day30 QEMU EDU coherent DMA mmap zero-copy driver");
MODULE_LICENSE("GPL");

/*
 * ==================== 附录：mmap 零拷贝数据流图 ====================
 *
 * 【没有 mmap 时，用户态访问 DMA buffer】
 *
 *   用户态                内核                    DMA buffer
 *      │                   │                        │
 *      │  copy_from_user   │                        │
 *      │──────────────────→│                        │
 *      │                   │                        │
 *      │              [内核访问]                    │
 *      │                   │                        │
 *      │  copy_to_user     │                        │
 *      │←──────────────────│                        │
 *      │                   │                        │
 *   用户态 ←              │                    DMA buffer
 *   需要两次 copy
 *
 *
 * 【有了 mmap 后，用户态直接访问 DMA buffer】
 *
 *   用户态          内核（建立映射）          DMA buffer
 *      │                   │                        │
 *      │  mmap()           │                        │
 *      │──────────────────→│                        │
 *      │  返回虚拟地址      │                        │
 *      │←──────────────────│                        │
 *      │                   │                        │
 *      │  [直接访问]        │                        │
 *      │──────────────────────────→（页表映射）     │
 *      │←─────────────────────────────────────────│
 *      │                   │                        │
 *   用户态 ← 直接访问！     │                    DMA buffer
 *   零次 copy
 */
