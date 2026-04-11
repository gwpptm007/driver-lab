// SPDX-License-Identifier: GPL-2.0
/*
 * Day31 - QEMU EDU bench 驱动
 *
 * ==================== 驱动目标 ====================
 *
 * Day31 承接 Day30 的 mmap 零拷贝基础，引入性能基准测试（Bench）。
 *
 * 【Day30 vs Day31 的核心区别】
 *   Day30：验证 mmap 零拷贝能工作（功能测试）
 *   Day31：测量这链路有多快/多稳（性能测试）
 *
 * 【Day31 的职责划分】
 *   用户态负责：计时、统计、分位数计算
 *   内核负责：提供 last_run_ns（内核视角的纯 DMA 耗时）
 *
 * ==================== 头文件依赖 ====================
 *
 * 新增时间相关：
 *   linux/ktime.h → ktime_get_ns()（纳秒精度计时）
 *
 * 其他与 Day30 相同：
 *   DMA：linux/dma-mapping.h
 *   字符设备：linux/cdev.h, linux/fs.h
 *   PCI：linux/pci.h
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

#include "include/day31_edu_bench.h"
#include "../include/day31_edu_uapi.h"

/*
 * ==================== 模块全局变量 ====================
 *
 * g_day31_base_dev：分配到的设备号范围
 * g_day31_class：sysfs class 指针
 * g_day31_minor：次设备号计数器
 * dma_mask_bits：可加载参数，默认 32
 *
 * 【新增】bench_verbose：热路径日志开关
 *   默认关闭（0），避免 IRQ handler 打印拖慢自动化
 */
static dev_t g_day31_base_dev;
static struct class *g_day31_class;
static atomic_t g_day31_minor = ATOMIC_INIT(0);

static unsigned int dma_mask_bits = DAY31_DMA_MASK_BITS_DEFAULT;
module_param(dma_mask_bits, uint, 0644);
MODULE_PARM_DESC(dma_mask_bits,
		 "DMA mask bits for QEMU EDU (default 32 in day31 automation)");

/*
 * bench_verbose：热路径日志开关
 *
 * 为什么需要这个参数？
 *   bench-dma 每次运行触发两次 IRQ
 *   如果每次 IRQ 都 dev_info() 打印，在 QEMU -nographic 下会拖慢自动化
 *
 * 默认值：false（关闭）
 * 设置方式：insmod day31_edu_bench.ko bench_verbose=1
 */
static bool bench_verbose;
module_param(bench_verbose, bool, 0644);
MODULE_PARM_DESC(bench_verbose,
		 "Enable verbose hot-path logging for irq/run_dma (default false)");

/*
 * ==================== 寄存器读写辅助函数 ====================
 *
 * 与 Day30 完全相同：
 *   day31_read32：读 32-bit 寄存器
 *   day31_write32：写 32-bit 寄存器
 *   day31_write64：写 64-bit 寄存器（DMA 地址）
 */
static inline u32 day31_read32(struct day31_dev *d, u32 off)
{
	return readl(d->bar0 + off);
}

static inline void day31_write32(struct day31_dev *d, u32 off, u32 val)
{
	writel(val, d->bar0 + off);
}

static inline void day31_write64(struct day31_dev *d, u32 off, u64 val)
{
	writeq(val, d->bar0 + off);
}

/*
 * ==================== 中断处理函数 ====================
 *
 * 与 Day30 的区别：
 *   - hot-path 日志默认关闭（bench_verbose 控制）
 *   - 其他逻辑完全相同
 */
static irqreturn_t day31_irq_handler(int irq, void *opaque)
{
	struct day31_dev *d = opaque;
	unsigned long flags;
	u32 status;

	status = day31_read32(d, DAY31_EDU_REG_IRQ_STATUS);
	if (!status)
		return IRQ_NONE;

	/*
	 * 自旋锁保护共享数据
	 */
	spin_lock_irqsave(&d->irq_lock, flags);
	d->irq_count++;
	d->last_irq_status = status;
	d->last_ack_value = status;
	spin_unlock_irqrestore(&d->irq_lock, flags);

	day31_write32(d, DAY31_EDU_REG_IRQ_ACK, status);

	/*
	 * 【Day31 变化】bench_verbose 控制 hot-path 日志
	 *
	 * bench-dma 每次运行触发两次 IRQ
	 * 关闭 verbose 可避免日志拖慢自动化
	 */
	if (unlikely(bench_verbose))
		dev_info(&d->pdev->dev,
			 "irq handler: irq=%d status=0x%08x count=%llu\n",
			 irq, status, d->irq_count);
	return IRQ_HANDLED;
}

/*
 * ==================== DMA 等待与编程 ====================
 *
 * 与 Day30 完全相同
 */
static int day31_wait_dma_idle(struct day31_dev *d)
{
	int i;
	u32 cmd;

	for (i = 0; i < 50000; ++i) {
		cmd = day31_read32(d, DAY31_EDU_REG_DMA_CMD);
		if (!(cmd & DAY31_EDU_DMA_CMD_START))
			return 0;
		udelay(10);
	}

	dev_err(&d->pdev->dev, "DMA timeout: cmd=0x%08x\n",
		day31_read32(d, DAY31_EDU_REG_DMA_CMD));
	return -ETIMEDOUT;
}

static int day31_program_dma(struct day31_dev *d, u64 src, u64 dst,
			     u32 count, u32 cmd)
{
	if (!count)
		return -EINVAL;

	d->last_dma_cmd = cmd;
	day31_write64(d, DAY31_EDU_REG_DMA_SRC, src);
	day31_write64(d, DAY31_EDU_REG_DMA_DST, dst);
	day31_write32(d, DAY31_EDU_REG_DMA_COUNT, count);
	day31_write32(d, DAY31_EDU_REG_DMA_CMD, cmd);
	return day31_wait_dma_idle(d);
}

/*
 * ==================== 状态记录函数 ====================
 *
 * day31_reset_run_result：重置 DMA 运行结果
 * day31_record_mmap_result：记录 mmap 结果
 *
 * 与 Day30 的区别：
 *   - day31_reset_run_result 新增 last_run_ns 重置
 */
static void day31_reset_run_result(struct day31_dev *d)
{
	d->last_run_ns = 0;
	d->last_run_len = 0;
	d->last_run_seed = 0;
	d->last_run_error = 0;
	d->last_run_ok = 0;
	d->last_irq_delta = 0;
	d->last_dma_cmd = 0;
}

static void day31_record_mmap_result(struct day31_dev *d, bool ok,
				     int err, unsigned long len,
				     unsigned long pgoff)
{
	d->last_mmap_ok = ok ? 1U : 0U;
	d->last_mmap_error = err;
	d->last_mmap_len = (u32)len;
	d->last_mmap_pgoff = (u32)pgoff;
}

/*
 * ==================== 【核心】DMA 运行 + 计时 ====================
 *
 * day31_do_run_dma：执行两段 DMA 往返 + 纳秒精度计时
 *
 * 【与 Day30 day30_do_run_dma 的区别】
 *   Day30：不计时，只发起 DMA
 *   Day31：使用 ktime_get_ns() 精确计时，记录 last_run_ns
 *
 * 【时间精度】
 *   ktime_get_ns() 返回纳秒（10^-9 秒）
 *   DMA 往返约 200μs = 200,000ns
 *   纳秒精度足够测量
 */
static int day31_do_run_dma(struct day31_dev *d, u32 len, u32 seed)
{
	u64 src_dma;
	u64 dst_dma;
	u64 irq_before;
	u64 start_ns;
	u64 end_ns;
	int ret;

	if (!d->dma_virt)
		return -ENODEV;
	if (!len || len > DAY31_DMA_VERIFY_MAX)
		return -EINVAL;

	mutex_lock(&d->op_lock);

	/*
	 * 重置运行结果并更新统计
	 */
	day31_reset_run_result(d);
	d->total_run_calls++;  /* 【新增】累计调用计数 */
	d->last_run_len = len;
	d->last_run_seed = seed;

	/*
	 * 计算 DMA 地址
	 */
	src_dma = (u64)d->dma_handle + DAY31_DMA_SRC_OFF;
	dst_dma = (u64)d->dma_handle + DAY31_DMA_DST_OFF;

	irq_before = d->irq_count;

	/*
	 * 【Day31 核心变化】纳秒精度计时
	 *
	 * start_ns 记录 DMA 开始时刻
	 * end_ns 记录 DMA 结束时刻
	 * last_run_ns = end_ns - start_ns
	 *
	 * 注意：这是"内核视角"的计时
	 * 不包括用户态 memcpy 和 ioctl syscall 开销
	 */
	if (unlikely(bench_verbose))
		dev_info(&d->pdev->dev,
			 "run_dma start: len=%u seed=0x%x src_dma=0x%llx dst_dma=0x%llx\n",
			 len, seed,
			 (unsigned long long)src_dma,
			 (unsigned long long)dst_dma);

	start_ns = ktime_get_ns();

	/*
	 * 第一次 DMA：RAM → EDU
	 */
	ret = day31_program_dma(d, src_dma, DAY31_EDU_DEVBUF_OFFSET,
				len,
				DAY31_EDU_DMA_CMD_START |
				DAY31_EDU_DMA_CMD_IRQ);
	if (ret) {
		d->last_run_error = ret;
		dev_err(&d->pdev->dev, "run_dma stage1 RAM->EDU failed: %d\n", ret);
		goto out;
	}

	/*
	 * 第二次 DMA：EDU → RAM
	 */
	ret = day31_program_dma(d, DAY31_EDU_DEVBUF_OFFSET, dst_dma,
				len,
				DAY31_EDU_DMA_CMD_START |
				DAY31_EDU_DMA_CMD_DIR_TO_RAM |
				DAY31_EDU_DMA_CMD_IRQ);
	if (ret) {
		d->last_run_error = ret;
		dev_err(&d->pdev->dev, "run_dma stage2 EDU->RAM failed: %d\n", ret);
		goto out;
	}

	/*
	 * 记录 DMA 结果和耗时
	 */
	d->last_irq_delta = (u32)(d->irq_count - irq_before);
	d->last_run_ok = 1;
	d->total_run_ok++;  /* 【新增】累计成功计数 */

	if (unlikely(bench_verbose))
		dev_info(&d->pdev->dev,
			 "run_dma ok: len=%u seed=0x%x irq_delta=%u\n",
			 len, seed, d->last_irq_delta);

out:
	/*
	 * 【Day31 核心】在互斥锁外计算耗时
	 * 确保 last_run_ns 反映真实的 DMA 执行时间
	 */
	end_ns = ktime_get_ns();
	d->last_run_ns = end_ns - start_ns;

	if (d->last_run_error)
		d->total_run_fail++;  /* 【新增】累计失败计数 */

	mutex_unlock(&d->op_lock);
	return d->last_run_error;
}

/*
 * ==================== 状态文本构建 ====================
 *
 * day31_build_state_text：生成可读状态字符串
 *
 * 与 Day30 的区别：
 *   - 新增 total_run_calls / total_run_ok / total_run_fail / last_run_ns
 */
static ssize_t day31_build_state_text(struct day31_dev *d, char *buf, size_t size)
{
	return scnprintf(buf, size,
			 "vendor=0x%04x device=0x%04x\n"
			 "bar0_start=0x%llx bar0_len=0x%llx\n"
			 "irq_vector=%u irq_count=%llu msi_enabled=%u\n"
			 "last_irq_status=0x%08x last_ack_value=0x%08x\n"
			 "dma_handle=0x%llx dma_bytes=%zu dma_mask_bits=%u\n"
			 "map_bytes=%lu src_off=%u dst_off=%u max_verify_len=%u\n"
			 "total_run_calls=%llu total_run_ok=%llu total_run_fail=%llu last_run_ns=%llu\n"
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
			 DAY31_DMA_SRC_OFF,
			 DAY31_DMA_DST_OFF,
			 DAY31_DMA_VERIFY_MAX,
			 d->total_run_calls,
			 d->total_run_ok,
			 d->total_run_fail,
			 d->last_run_ns,
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
 * day31_open：打开设备
 * day31_read：读取状态文本
 * day31_mmap：【核心】mmap coherent DMA buffer
 * day31_ioctl：设备控制
 *
 * 与 Day30 完全相同
 */
static int day31_open(struct inode *inode, struct file *file)
{
	struct day31_dev *d = container_of(inode->i_cdev, struct day31_dev, cdev);

	file->private_data = d;
	return 0;
}

static ssize_t day31_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct day31_dev *d = file->private_data;
	char kbuf[384];
	ssize_t len;

	if (!d || !d->bar0)
		return -ENODEV;

	len = day31_build_state_text(d, kbuf, sizeof(kbuf));
	return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

static int day31_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct day31_dev *d = file->private_data;
	unsigned long len = vma->vm_end - vma->vm_start;
	unsigned long map_bytes = PAGE_ALIGN(d->dma_bytes);
	int ret;

	if (!d || !d->dma_virt)
		return -ENODEV;

	/*
	 * 边界校验：只允许 offset=0 和 length=map_bytes
	 */
	if (vma->vm_pgoff != 0) {
		day31_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
		dev_err(&d->pdev->dev,
			"mmap rejected: pgoff=%lu expected=0\n",
			vma->vm_pgoff);
		return -EINVAL;
	}

	if (len != map_bytes) {
		day31_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
		dev_err(&d->pdev->dev,
			"mmap rejected: len=%lu expected=%lu\n",
			len, map_bytes);
		return -EINVAL;
	}

	vma->vm_flags |= VM_IO | VM_DONTDUMP | VM_DONTEXPAND;

	ret = dma_mmap_coherent(&d->pdev->dev, vma,
				d->dma_virt, d->dma_handle, d->dma_bytes);
	day31_record_mmap_result(d, ret == 0, ret, len, vma->vm_pgoff);

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
 * 与 Day30 相比：
 *   - 新增 total_run_calls / total_run_ok / total_run_fail / last_run_ns
 *   - ioctl 编号使用 'B' ('B' for Bench)
 *
 * 命令：
 *   DAY31_IOC_GET_INFO：获取完整信息（含 bench 统计）
 *   DAY31_IOC_RUN_DMA：触发 DMA + 计时
 *   DAY31_IOC_GET_RESULT：获取结果（含 last_run_ns）
 *   DAY31_IOC_RESET_STATS：重置所有统计
 */
static long day31_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct day31_dev *d = file->private_data;

	switch (cmd) {
	case DAY31_IOC_GET_INFO: {
		struct day31_info info = {
			.tool_api_version = DAY31_TOOL_API_VERSION,
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
			.src_off = DAY31_DMA_SRC_OFF,
			.dst_off = DAY31_DMA_DST_OFF,
			.max_verify_len = DAY31_DMA_VERIFY_MAX,
			.total_run_calls = d->total_run_calls,
			.total_run_ok = d->total_run_ok,
			.total_run_fail = d->total_run_fail,
			.last_run_ns = d->last_run_ns,
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
	case DAY31_IOC_RUN_DMA: {
		struct day31_run_req req;

		if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
			return -EFAULT;
		return day31_do_run_dma(d, req.len, req.pattern_seed);
	}
	case DAY31_IOC_GET_RESULT: {
		struct day31_run_result res = {
			.total_run_calls = d->total_run_calls,
			.total_run_ok = d->total_run_ok,
			.total_run_fail = d->total_run_fail,
			.last_run_ns = d->last_run_ns,
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
	case DAY31_IOC_RESET_STATS:
		mutex_lock(&d->op_lock);
		d->irq_count = 0;
		d->last_irq_status = 0;
		d->last_ack_value = 0;
		d->total_run_calls = 0;
		d->total_run_ok = 0;
		d->total_run_fail = 0;
		day31_reset_run_result(d);
		day31_record_mmap_result(d, false, 0, 0, 0);
		mutex_unlock(&d->op_lock);
		return 0;
	default:
		return -ENOTTY;
	}
}

/*
 * ==================== 文件操作集合定义 ====================
 */
static const struct file_operations day31_fops = {
	.owner          = THIS_MODULE,
	.open           = day31_open,
	.read           = day31_read,
	.mmap           = day31_mmap,
	.unlocked_ioctl = day31_ioctl,
	.llseek         = no_llseek,
};

/*
 * ==================== 字符设备注册/注销 ====================
 *
 * 与 Day30 完全相同
 */
static int day31_setup_chrdev(struct day31_dev *d)
{
	int minor;
	int ret;

	minor = atomic_fetch_add(1, &g_day31_minor);
	d->devt = MKDEV(MAJOR(g_day31_base_dev), minor);

	cdev_init(&d->cdev, &day31_fops);
	d->cdev.owner = THIS_MODULE;

	ret = cdev_add(&d->cdev, d->devt, 1);
	if (ret)
		return ret;

	d->device = device_create(g_day31_class, &d->pdev->dev, d->devt, NULL,
				  DAY31_DEV_NAME_FMT, minor);
	if (IS_ERR(d->device)) {
		ret = PTR_ERR(d->device);
		d->device = NULL;
		cdev_del(&d->cdev);
		return ret;
	}
	return 0;
}

static void day31_destroy_chrdev(struct day31_dev *d)
{
	if (d->device)
		device_destroy(g_day31_class, d->devt);
	cdev_del(&d->cdev);
}

/*
 * ==================== PCI probe/remove ====================
 *
 * 与 Day30 几乎完全相同
 * 区别：day31_dev 结构体多了 total_run_calls/ok/fail 字段
 *       这些字段在 kzalloc 时自动清零
 */
static int day31_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct day31_dev *d;
	u32 ident;
	u32 live;
	int ret;

	dev_info(&pdev->dev, "probe enter: %04x:%04x\n", pdev->vendor, pdev->device);

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	d->pdev = pdev;
	d->dma_mask_bits = dma_mask_bits;
	d->dma_bytes = DAY31_DMA_BYTES;
	spin_lock_init(&d->irq_lock);
	mutex_init(&d->op_lock);
	pci_set_drvdata(pdev, d);

	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "pci_enable_device failed: %d\n", ret);
		goto err_free;
	}

	ret = dma_set_mask_and_coherent(&pdev->dev,
					DMA_BIT_MASK(d->dma_mask_bits));
	if (ret) {
		dev_err(&pdev->dev,
			"dma_set_mask_and_coherent(%u bits) failed: %d\n",
			d->dma_mask_bits, ret);
		goto err_disable;
	}
	dev_info(&pdev->dev, "dma mask set to %u bits\n", d->dma_mask_bits);

	ret = pci_request_regions(pdev, DAY31_DRV_NAME);
	if (ret) {
		dev_err(&pdev->dev, "pci_request_regions failed: %d\n", ret);
		goto err_disable;
	}

	pci_set_master(pdev);

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

	ident = day31_read32(d, DAY31_EDU_REG_IDENTITY);
	live = day31_read32(d, DAY31_EDU_REG_LIVENESS);
	dev_info(&pdev->dev, "ident=0x%08x liveness=0x%08x\n", ident, live);

	/*
	 * 分配 coherent DMA buffer
	 * 与 Day30 完全相同
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

	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
	if (ret < 0) {
		dev_err(&pdev->dev, "pci_alloc_irq_vectors failed: %d\n", ret);
		goto err_dma;
	}
	d->irq_vector = pci_irq_vector(pdev, 0);

	ret = request_irq(d->irq_vector, day31_irq_handler, 0, DAY31_DRV_NAME, d);
	if (ret) {
		dev_err(&pdev->dev, "request_irq(%u) failed: %d\n",
			d->irq_vector, ret);
		goto err_irq_vectors;
	}
	dev_info(&pdev->dev, "MSI vector=%u enabled=%u\n",
		 d->irq_vector, !!pdev->msi_enabled);

	ret = day31_setup_chrdev(d);
	if (ret) {
		dev_err(&pdev->dev, "day31_setup_chrdev failed: %d\n", ret);
		goto err_irq;
	}

	day31_reset_run_result(d);
	day31_record_mmap_result(d, false, 0, 0, 0);
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

static void day31_remove(struct pci_dev *pdev)
{
	struct day31_dev *d = pci_get_drvdata(pdev);

	if (!d)
		return;

	dev_info(&pdev->dev, "remove enter\n");
	day31_destroy_chrdev(d);
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
static const struct pci_device_id day31_pci_ids[] = {
	{ PCI_DEVICE(DAY31_EDU_VENDOR_ID, DAY31_EDU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, day31_pci_ids);

static struct pci_driver day31_pci_driver = {
	.name = DAY31_DRV_NAME,
	.id_table = day31_pci_ids,
	.probe = day31_probe,
	.remove = day31_remove,
};

/*
 * ==================== 模块初始化/退出 ====================
 */
static int __init day31_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&g_day31_base_dev, 0, 256, DAY31_DRV_NAME);
	if (ret)
		return ret;

	g_day31_class = class_create(THIS_MODULE, DAY31_CLASS_NAME);
	if (IS_ERR(g_day31_class)) {
		ret = PTR_ERR(g_day31_class);
		unregister_chrdev_region(g_day31_base_dev, 256);
		return ret;
	}

	ret = pci_register_driver(&day31_pci_driver);
	if (ret) {
		class_destroy(g_day31_class);
		unregister_chrdev_region(g_day31_base_dev, 256);
		return ret;
	}

	pr_info(DAY31_DRV_NAME ": init ok\n");
	return 0;
}

static void __exit day31_exit(void)
{
	pci_unregister_driver(&day31_pci_driver);
	class_destroy(g_day31_class);
	unregister_chrdev_region(g_day31_base_dev, 256);
	pr_info(DAY31_DRV_NAME ": exit ok\n");
}

module_init(day31_init);
module_exit(day31_exit);

MODULE_AUTHOR("Richer Wong");
MODULE_DESCRIPTION("Day31 QEMU EDU bench driver");
MODULE_LICENSE("GPL");

/*
 * ==================== 附录：Bench 三条路径性能对比 ====================
 *
 * 【路径 A：ioctl 控制路径】
 *   操作：ioctl(GET_INFO)
 *   测量内容：用户态 → 内核态 → 返回
 *   典型耗时：~16μs
 *
 *   用户态计时                    内核视角
 *   ├─ ioctl syscall              │
 *   │                             ├─ GET_INFO 处理
 *   │                             │
 *   └─ 返回                       └─ 返回
 *
 *
 * 【路径 B：mmap 用户态路径】
 *   操作：memcpy + memcmp（纯用户态）
 *   测量内容：直接内存访问速度
 *   典型耗时：~0.5μs，吞吐 ~887MB/s
 *
 *   用户态计时
 *   ├─ memcpy(dst, src, len)
 *   ├─ memcmp(src, dst, len)
 *   └─ 返回
 *
 *   无内核参与！
 *
 *
 * 【路径 C：DMA 端到端路径】
 *   操作：ioctl(RUN_DMA) + memcmp
 *   测量内容：用户态 + 内核 DMA + 设备
 *   典型耗时：~200ms（QEMU EDU 软件模拟，较慢）
 *
 *   用户态计时                    内核视角              EDU 设备
 *   ├─ fill_pattern              │                     │
 *   ├─ memset                   │                     │
 *   ├─ ioctl syscall            │                     │
 *   │                           ├─ DMA 编程            │
 *   │                           │                     │
 *   │                           ├─ 等待 DMA 完成       │
 *   │                           │                     │
 *   │                           └─ last_run_ns        │
 *   │                                                │
 *   ├─ memcmp                  │                     │
 *   └─ 返回                     │                     │
 *
 *
 * 【last_run_ns 的意义】
 *
 *   last_run_ns 只包含"内核视角的纯 DMA 耗时"
 *   不包括：用户态 memcpy/ioctl syscall/ memcmp
 *
 *   这样可以分析：
 *     - ioctl 总耗时 ≈ syscall 开销 + 内核 DMA 耗时
 *     - 如果 ioctl 耗时 >> last_run_ns，说明瓶颈在 syscall
 *     - 如果 ioctl 耗时 ≈ last_run_ns，说明瓶颈在 DMA
 */
