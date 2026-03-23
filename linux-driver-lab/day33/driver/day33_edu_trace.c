// SPDX-License-Identifier: GPL-2.0
/*
 * Day33 - QEMU EDU ftrace baseline driver
 *
 * Day33 不再新增复杂功能，而是把 day32 已通过的主链路拿来做 function_graph 解释。
 * 因此驱动能力基本延续 day32：
 * 1. coherent DMA + mmap 基线保持不变；
 * 2. ioctl / mmap / run_dma 接口继续可用；
 * 3. 默认关闭 hot-path 日志，避免串口输出把 trace 结果本身拖慢。
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

#include "include/day33_edu_trace.h"
#include "../include/day33_edu_uapi.h"

static dev_t g_day33_base_dev;
static struct class *g_day33_class;
static atomic_t g_day33_minor = ATOMIC_INIT(0);

static unsigned int dma_mask_bits = DAY33_DMA_MASK_BITS_DEFAULT;
module_param(dma_mask_bits, uint, 0644);
MODULE_PARM_DESC(dma_mask_bits,
		 "DMA mask bits for QEMU EDU (default 32 in day33 automation)");

static bool trace_verbose;
module_param(trace_verbose, bool, 0644);
MODULE_PARM_DESC(trace_verbose,
		 "Enable verbose hot-path logging for irq/run_dma (default false)");

static inline u32 day33_read32(struct day33_dev *d, u32 off)
{
	return readl(d->bar0 + off);
}

static inline void day33_write32(struct day33_dev *d, u32 off, u32 val)
{
	writel(val, d->bar0 + off);
}

static inline void day33_write64(struct day33_dev *d, u32 off, u64 val)
{
	writeq(val, d->bar0 + off);
}

static irqreturn_t day33_irq_handler(int irq, void *opaque)
{
	struct day33_dev *d = opaque;
	unsigned long flags;
	u32 status;

	status = day33_read32(d, DAY33_EDU_REG_IRQ_STATUS);
	if (!status)
		return IRQ_NONE;

	spin_lock_irqsave(&d->irq_lock, flags);
	d->irq_count++;
	d->last_irq_status = status;
	d->last_ack_value = status;
	spin_unlock_irqrestore(&d->irq_lock, flags);

	day33_write32(d, DAY33_EDU_REG_IRQ_ACK, status);
	/*
	 * day33 默认关闭 hot-path 日志。
	 * 原因：bench-dma 每次 run 都会触发两次 IRQ，若每次 IRQ 都打印，
	 * 在 -nographic 的 QEMU 场景下会显著拖慢自动化。
	 */
	if (unlikely(trace_verbose))
		dev_info(&d->pdev->dev,
			 "irq handler: irq=%d status=0x%08x count=%llu\n",
			 irq, status, d->irq_count);
	return IRQ_HANDLED;
}

static int day33_wait_dma_idle(struct day33_dev *d)
{
	int i;
	u32 cmd;

	for (i = 0; i < 50000; ++i) {
		cmd = day33_read32(d, DAY33_EDU_REG_DMA_CMD);
		if (!(cmd & DAY33_EDU_DMA_CMD_START))
			return 0;
		udelay(10);
	}

	dev_err(&d->pdev->dev, "DMA timeout: cmd=0x%08x\n",
		day33_read32(d, DAY33_EDU_REG_DMA_CMD));
	return -ETIMEDOUT;
}

static int day33_program_dma(struct day33_dev *d, u64 src, u64 dst,
			     u32 count, u32 cmd)
{
	if (!count)
		return -EINVAL;

	d->last_dma_cmd = cmd;
	day33_write64(d, DAY33_EDU_REG_DMA_SRC, src);
	day33_write64(d, DAY33_EDU_REG_DMA_DST, dst);
	day33_write32(d, DAY33_EDU_REG_DMA_COUNT, count);
	day33_write32(d, DAY33_EDU_REG_DMA_CMD, cmd);
	return day33_wait_dma_idle(d);
}

static void day33_reset_run_result(struct day33_dev *d)
{
	d->last_run_ns = 0;
	d->last_run_len = 0;
	d->last_run_seed = 0;
	d->last_run_error = 0;
	d->last_run_ok = 0;
	d->last_irq_delta = 0;
	d->last_dma_cmd = 0;
}

static void day33_record_mmap_result(struct day33_dev *d, bool ok,
				     int err, unsigned long len,
				     unsigned long pgoff)
{
	d->last_mmap_ok = ok ? 1U : 0U;
	d->last_mmap_error = err;
	d->last_mmap_len = (u32)len;
	d->last_mmap_pgoff = (u32)pgoff;
}

static int day33_do_run_dma(struct day33_dev *d, u32 len, u32 seed)
{
	u64 src_dma;
	u64 dst_dma;
	u64 irq_before;
	u64 start_ns;
	u64 end_ns;
	int ret;

	if (!d->dma_virt)
		return -ENODEV;
	if (!len || len > DAY33_DMA_VERIFY_MAX)
		return -EINVAL;

	mutex_lock(&d->op_lock);
	day33_reset_run_result(d);
	d->total_run_calls++;
	d->last_run_len = len;
	d->last_run_seed = seed;

	src_dma = (u64)d->dma_handle + DAY33_DMA_SRC_OFF;
	dst_dma = (u64)d->dma_handle + DAY33_DMA_DST_OFF;
	irq_before = d->irq_count;

	/*
	 * Day33 延续 day30 的核心设计：
	 * - buffer 内容依然由用户态通过 mmap 去准备与验证；
	 * - 内核尽量只负责 DMA 发起、等待完成与最小统计。
	 *
	 * 这样 bench 结果才能把“用户态内存处理成本”和“设备参与成本”分开看。
	 */
	if (unlikely(trace_verbose))
		dev_info(&d->pdev->dev,
			 "run_dma start: len=%u seed=0x%x src_dma=0x%llx dst_dma=0x%llx\n",
			 len, seed,
			 (unsigned long long)src_dma,
			 (unsigned long long)dst_dma);

	start_ns = ktime_get_ns();

	ret = day33_program_dma(d, src_dma, DAY33_EDU_DEVBUF_OFFSET,
				len,
				DAY33_EDU_DMA_CMD_START |
				DAY33_EDU_DMA_CMD_IRQ);
	if (ret) {
		d->last_run_error = ret;
		dev_err(&d->pdev->dev, "run_dma stage1 RAM->EDU failed: %d\n", ret);
		goto out;
	}

	ret = day33_program_dma(d, DAY33_EDU_DEVBUF_OFFSET, dst_dma,
				len,
				DAY33_EDU_DMA_CMD_START |
				DAY33_EDU_DMA_CMD_DIR_TO_RAM |
				DAY33_EDU_DMA_CMD_IRQ);
	if (ret) {
		d->last_run_error = ret;
		dev_err(&d->pdev->dev, "run_dma stage2 EDU->RAM failed: %d\n", ret);
		goto out;
	}

	/*
	 * EDU round-trip 的理想情况是两次 IRQ：
	 * 1) RAM -> EDU internal buffer
	 * 2) EDU internal buffer -> RAM
	 */
	d->last_irq_delta = (u32)(d->irq_count - irq_before);
	d->last_run_ok = 1;
	d->total_run_ok++;
	if (unlikely(trace_verbose))
		dev_info(&d->pdev->dev,
			 "run_dma ok: len=%u seed=0x%x irq_delta=%u\n",
			 len, seed, d->last_irq_delta);

out:
	end_ns = ktime_get_ns();
	d->last_run_ns = end_ns - start_ns;
	if (d->last_run_error)
		d->total_run_fail++;
	mutex_unlock(&d->op_lock);
	return d->last_run_error;
}

static ssize_t day33_build_state_text(struct day33_dev *d, char *buf, size_t size)
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
			 DAY33_DMA_SRC_OFF,
			 DAY33_DMA_DST_OFF,
			 DAY33_DMA_VERIFY_MAX,
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

static int day33_open(struct inode *inode, struct file *file)
{
	struct day33_dev *d = container_of(inode->i_cdev, struct day33_dev, cdev);

	file->private_data = d;
	return 0;
}

static ssize_t day33_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct day33_dev *d = file->private_data;
	char kbuf[384];
	ssize_t len;

	if (!d || !d->bar0)
		return -ENODEV;

	len = day33_build_state_text(d, kbuf, sizeof(kbuf));
	return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

static int day33_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct day33_dev *d = file->private_data;
	unsigned long len = vma->vm_end - vma->vm_start;
	unsigned long map_bytes = PAGE_ALIGN(d->dma_bytes);
	int ret;

	if (!d || !d->dma_virt)
		return -ENODEV;

	/*
	 * Day33 继续只支持“整页映射 + offset=0”：
	 * - 这样 bench 的变量更少，路径定义更稳定；
	 * - 同时保留清晰的非法映射失败路径，便于后续验证边界。
	 */
	if (vma->vm_pgoff != 0) {
		day33_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
		dev_err(&d->pdev->dev,
			"mmap rejected: pgoff=%lu expected=0\n",
			vma->vm_pgoff);
		return -EINVAL;
	}

	if (len != map_bytes) {
		day33_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
		dev_err(&d->pdev->dev,
			"mmap rejected: len=%lu expected=%lu\n",
			len, map_bytes);
		return -EINVAL;
	}

	vma->vm_flags |= VM_IO | VM_DONTDUMP | VM_DONTEXPAND;

	ret = dma_mmap_coherent(&d->pdev->dev, vma,
				d->dma_virt, d->dma_handle, d->dma_bytes);
	day33_record_mmap_result(d, ret == 0, ret, len, vma->vm_pgoff);
	if (ret)
		dev_err(&d->pdev->dev, "dma_mmap_coherent failed: %d\n", ret);
	else
		dev_info(&d->pdev->dev,
			 "mmap ok: len=%lu pgoff=%lu dma=0x%llx\n",
			 len, vma->vm_pgoff,
			 (unsigned long long)d->dma_handle);
	return ret;
}

static long day33_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct day33_dev *d = file->private_data;

	switch (cmd) {
	case DAY33_IOC_GET_INFO: {
		struct day33_info info = {
			.tool_api_version = DAY33_TOOL_API_VERSION,
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
			.src_off = DAY33_DMA_SRC_OFF,
			.dst_off = DAY33_DMA_DST_OFF,
			.max_verify_len = DAY33_DMA_VERIFY_MAX,
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
	case DAY33_IOC_RUN_DMA: {
		struct day33_run_req req;

		if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
			return -EFAULT;
		return day33_do_run_dma(d, req.len, req.pattern_seed);
	}
	case DAY33_IOC_GET_RESULT: {
		struct day33_run_result res = {
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
	case DAY33_IOC_RESET_STATS:
		mutex_lock(&d->op_lock);
		d->irq_count = 0;
		d->last_irq_status = 0;
		d->last_ack_value = 0;
		d->total_run_calls = 0;
		d->total_run_ok = 0;
		d->total_run_fail = 0;
		day33_reset_run_result(d);
		day33_record_mmap_result(d, false, 0, 0, 0);
		mutex_unlock(&d->op_lock);
		return 0;
	default:
		return -ENOTTY;
	}
}

static const struct file_operations day33_fops = {
	.owner          = THIS_MODULE,
	.open           = day33_open,
	.read           = day33_read,
	.mmap           = day33_mmap,
	.unlocked_ioctl = day33_ioctl,
	.llseek         = no_llseek,
};

static int day33_setup_chrdev(struct day33_dev *d)
{
	int minor;
	int ret;

	minor = atomic_fetch_add(1, &g_day33_minor);
	d->devt = MKDEV(MAJOR(g_day33_base_dev), minor);

	cdev_init(&d->cdev, &day33_fops);
	d->cdev.owner = THIS_MODULE;

	ret = cdev_add(&d->cdev, d->devt, 1);
	if (ret)
		return ret;

	d->device = device_create(g_day33_class, &d->pdev->dev, d->devt, NULL,
				  DAY33_DEV_NAME_FMT, minor);
	if (IS_ERR(d->device)) {
		ret = PTR_ERR(d->device);
		d->device = NULL;
		cdev_del(&d->cdev);
		return ret;
	}
	return 0;
}

static void day33_destroy_chrdev(struct day33_dev *d)
{
	if (d->device)
		device_destroy(g_day33_class, d->devt);
	cdev_del(&d->cdev);
}

static int day33_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct day33_dev *d;
	u32 ident;
	u32 live;
	int ret;

	dev_info(&pdev->dev, "probe enter: %04x:%04x\n", pdev->vendor, pdev->device);

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	d->pdev = pdev;
	d->dma_mask_bits = dma_mask_bits;
	d->dma_bytes = DAY33_DMA_BYTES;
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

	ret = pci_request_regions(pdev, DAY33_DRV_NAME);
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

	ident = day33_read32(d, DAY33_EDU_REG_IDENTITY);
	live = day33_read32(d, DAY33_EDU_REG_LIVENESS);
	dev_info(&pdev->dev, "ident=0x%08x liveness=0x%08x\n", ident, live);

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

	ret = request_irq(d->irq_vector, day33_irq_handler, 0, DAY33_DRV_NAME, d);
	if (ret) {
		dev_err(&pdev->dev, "request_irq(%u) failed: %d\n",
			d->irq_vector, ret);
		goto err_irq_vectors;
	}
	dev_info(&pdev->dev, "MSI vector=%u enabled=%u\n",
		 d->irq_vector, !!pdev->msi_enabled);

	ret = day33_setup_chrdev(d);
	if (ret) {
		dev_err(&pdev->dev, "day33_setup_chrdev failed: %d\n", ret);
		goto err_irq;
	}

	day33_reset_run_result(d);
	day33_record_mmap_result(d, false, 0, 0, 0);
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

static void day33_remove(struct pci_dev *pdev)
{
	struct day33_dev *d = pci_get_drvdata(pdev);

	if (!d)
		return;

	dev_info(&pdev->dev, "remove enter\n");
	day33_destroy_chrdev(d);
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

static const struct pci_device_id day33_pci_ids[] = {
	{ PCI_DEVICE(DAY33_EDU_VENDOR_ID, DAY33_EDU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, day33_pci_ids);

static struct pci_driver day33_pci_driver = {
	.name = DAY33_DRV_NAME,
	.id_table = day33_pci_ids,
	.probe = day33_probe,
	.remove = day33_remove,
};

static int __init day33_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&g_day33_base_dev, 0, 256, DAY33_DRV_NAME);
	if (ret)
		return ret;

	g_day33_class = class_create(THIS_MODULE, DAY33_CLASS_NAME);
	if (IS_ERR(g_day33_class)) {
		ret = PTR_ERR(g_day33_class);
		unregister_chrdev_region(g_day33_base_dev, 256);
		return ret;
	}

	ret = pci_register_driver(&day33_pci_driver);
	if (ret) {
		class_destroy(g_day33_class);
		unregister_chrdev_region(g_day33_base_dev, 256);
		return ret;
	}

	pr_info(DAY33_DRV_NAME ": init ok\n");
	return 0;
}

static void __exit day33_exit(void)
{
	pci_unregister_driver(&day33_pci_driver);
	class_destroy(g_day33_class);
	unregister_chrdev_region(g_day33_base_dev, 256);
	pr_info(DAY33_DRV_NAME ": exit ok\n");
}

module_init(day33_init);
module_exit(day33_exit);

MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Day33 QEMU EDU bench driver");
MODULE_LICENSE("GPL");
