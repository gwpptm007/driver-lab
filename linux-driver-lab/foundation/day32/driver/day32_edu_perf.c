// SPDX-License-Identifier: GPL-2.0
/*
 * day32_edu_perf.c - Day32 QEMU EDU perf/ftrace 驱动
 *
 * ==================== 驱动作用 ====================
 *
 * Day32 在 Day31 的基础上继续延伸：
 *   Day30：mmap 零拷贝链路（用户态直接访问 DMA buffer）
 *   Day31：mmap 基准测试（建立 avg/p50/p95/p99/throughput 基线）
 *   Day32：perf/ftrace 热点分析 + 最小优化验证
 *
 * 驱动本身的核心功能与 Day31 完全相同：
 *   - PCI 设备探测与 BAR0 MMIO 映射
 *   - MSI 中断注册与处理（两次 DMA IRQ）
 *   - coherent DMA buffer 分配与 mmap 映射
 *   - ioctl RUN_DMA 触发两段 DMA 往返
 *   - 最小 bench 统计（total_run_calls/ok/fail/last_run_ns）
 *
 * Day32 真正的主角是：
 *   1. 用户态 bench 工具（baseline vs optimized 两条路径）
 *   2. 宿主端 perf/ftrace（采集热点、验证优化效果）
 *
 * ==================== 与 Day31 的区别 ====================
 *
 * Day32 驱动与 Day31 驱动几乎完全相同，区别仅在于：
 *   - 模块参数名：perf_verbose（而非 bench_verbose）
 *   - 模块描述：强调 perf/ftrace 主题
 *   - 其他所有逻辑与 Day31 相同
 *
 * Day32 的优化点在用户态 bench 工具，不在驱动：
 *   baseline：每轮都 GET_INFO + mmap + munmap
 *   optimized：提前一次 GET_INFO + mmap，循环内只 memcpy/compare
 *
 * ==================== 头文件依赖 ====================
 *
 * #include Linux 内核核心头文件（按功能分组）：
 *
 *   字符设备与 file_operations：
 *     linux/cdev.h     → struct cdev, cdev_init(), cdev_add()
 *     linux/fs.h       → struct file_operations, struct file
 *
 *   设备模型与类：
 *     linux/device.h   → struct class, device_create(), class_destroy()
 *
 *   内存管理：
 *     linux/mm.h       → struct vm_area_struct, VM_IO, VM_DONTEXPAND
 *     linux/dma-mapping.h → dma_alloc_coherent(), dma_mmap_coherent()
 *
 *   PCI 总线：
 *     linux/pci.h      → struct pci_dev, pci_enable_device(),
 *                        pci_iomap(), pci_set_master()
 *
 *   中断处理：
 *     linux/interrupt.h → IRQ_HANDLED, request_irq(), free_irq()
 *
 *   同步原语：
 *     linux/mutex.h    → struct mutex, mutex_init/lock/unlock
 *     linux/spinlock.h → struct spinlock, spin_lock_irqsave()
 *
 *   时间测量：
 *     linux/ktime.h    → ktime_get_ns()（纳秒精度计时）
 *
 *   其他：
 *     linux/io.h       → readl(), writel(), writeq()
 *     linux/slab.h     → kzalloc(), kfree()
 *     linux/uaccess.h  → copy_to_user(), copy_from_user()
 *     linux/module.h   → module_init/exit, module_param()
 *     linux/delay.h    → udelay()
 *
 * ==================== Section 1: 模块参数 ====================
 *
 * Day32 继续用 perf_verbose 控制 IRQ 热路径日志：
 *
 *   insmod day32_edu_perf.ko              # 默认关闭，自动化友好
 *   insmod day32_edu_perf.ko perf_verbose=1 # 开启，用于调试
 *
 * 为什么叫 perf_verbose 而不是 bench_verbose？
 *   Day32 的主角是 perf/ftrace，日志开关反映这个主题。
 *   语义相同，只是名称反映 Day32 的 perf 焦点。
 *
 * dma_mask_bits 继续沿用 32 作为默认值：
 *   Day30 自动化放宽了 DMA mask 要求，
 *   Day32 继续沿用这个设定，避免不必要的自动化失败。
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

#include "include/day32_edu_perf.h"
#include "../include/day32_edu_uapi.h"

/*
 * ==================== Section 2: 全局资源 ====================
 *
 * 与 Day31 完全相同的全局字符设备管理模式：
 *
 *   g_day32_base_dev：动态分配的主设备号（alloc_chrdev_region）
 *   g_day32_class：sysfs 类（class_create）
 *   g_day32_minor：原子计数器，用于分配次设备号
 *
 * 这种模式支持最多 256 个 day32 设备实例。
 */
static dev_t g_day32_base_dev;
static struct class *g_day32_class;
static atomic_t g_day32_minor = ATOMIC_INIT(0);

/*
 * Day32 继续沿用 32-bit DMA mask：
 * QEMU EDU 软件模拟环境不需要高端 DMA mask。
 */
static unsigned int dma_mask_bits = DAY32_DMA_MASK_BITS_DEFAULT;
module_param(dma_mask_bits, uint, 0644);
MODULE_PARM_DESC(dma_mask_bits,
		 "DMA mask bits for QEMU EDU (default 32 in day32 automation)");

/*
 * perf_verbose：控制 IRQ 热路径是否打印日志
 *
 * 为什么需要关闭？
 *   baseline 每次迭代都会触发 2 次 IRQ（两段 DMA）
 *   如果每次 IRQ 都 dev_info() 打印，
 *   在 QEMU -nographic 下会拖慢自动化和 perf 采样。
 *
 * 为什么叫 perf_verbose？
 *   Day32 主角是 perf/ftrace，这个名称反映主题。
 *   语义上等同于 Day31 的 bench_verbose。
 */
static bool perf_verbose;
module_param(perf_verbose, bool, 0644);
MODULE_PARM_DESC(perf_verbose,
		 "Enable verbose hot-path logging for irq/run_dma (default false)");

/*
 * ==================== Section 3: MMIO 访问辅助函数 ====================
 *
 * 与 Day31 完全相同：
 *   day32_read32()：读 32-bit MMIO 寄存器
 *   day32_write32()：写 32-bit MMIO 寄存器
 *   day32_write64()：写 64-bit MMIO 寄存器（用于 DMA 地址）
 *
 * 为什么需要 inline？
 *   这些是高频调用路径，inline 可以避免函数调用开销。
 *   同时保持代码清晰，便于阅读。
 */
static inline u32 day32_read32(struct day32_dev *d, u32 off)
{
	return readl(d->bar0 + off);
}

static inline void day32_write32(struct day32_dev *d, u32 off, u32 val)
{
	writel(val, d->bar0 + off);
}

static inline void day32_write64(struct day32_dev *d, u32 off, u64 val)
{
	writeq(val, d->bar0 + off);
}

/*
 * ==================== Section 4: IRQ 中断处理 ====================
 *
 * 与 Day31 完全相同的中断处理逻辑：
 *
 *   1. 读取 IRQ_STATUS 判断是否有未处理中断
 *   2. 原子更新 irq_count、last_irq_status、last_ack_value
 *   3. ACK 中断（向 IRQ_ACK 写入 status）
 *   4. perf_verbose 控制是否打印热路径日志
 *
 * 【为什么要 spin_lock_irqsave？】
 *   IRQ handler 可能在任意上下文被调用，
 *   需要用 spinlock 保护共享数据（irq_count 等），
 *   同时禁用本地中断（irqsave）防止死锁。
 *
 * 【day32 默认关闭 perf_verbose】
 *   如果开启，每次 DMA 往返（2 次 IRQ）都会打印，
 *   这会显著拖慢 baseline 性能测试和 perf 采样。
 */
static irqreturn_t day32_irq_handler(int irq, void *opaque)
{
	struct day32_dev *d = opaque;
	unsigned long flags;
	u32 status;

	status = day32_read32(d, DAY32_EDU_REG_IRQ_STATUS);
	if (!status)
		return IRQ_NONE;

	spin_lock_irqsave(&d->irq_lock, flags);
	d->irq_count++;
	d->last_irq_status = status;
	d->last_ack_value = status;
	spin_unlock_irqrestore(&d->irq_lock, flags);

	day32_write32(d, DAY32_EDU_REG_IRQ_ACK, status);

	if (unlikely(perf_verbose))
		dev_info(&d->pdev->dev,
			 "irq handler: irq=%d status=0x%08x count=%llu\n",
			 irq, status, d->irq_count);
	return IRQ_HANDLED;
}

/*
 * ==================== Section 5: DMA 编程与等待 ====================
 *
 * day32_wait_dma_idle()：
 *   轮询 DMA_CMD 寄存器的 START 位，等待 DMA 完成。
 *   最多等待 50000 * 10us = 500ms。
 *   超时返回 -ETIMEDOUT。
 *
 * day32_program_dma()：
 *   写入 DMA 寄存器（src/dst/count/cmd），
 *   然后等待 DMA 完成。
 *
 * 与 Day31 完全相同。
 */
static int day32_wait_dma_idle(struct day32_dev *d)
{
	int i;
	u32 cmd;

	for (i = 0; i < 50000; ++i) {
		cmd = day32_read32(d, DAY32_EDU_REG_DMA_CMD);
		if (!(cmd & DAY32_EDU_DMA_CMD_START))
			return 0;
		udelay(10);
	}

	dev_err(&d->pdev->dev, "DMA timeout: cmd=0x%08x\n",
		day32_read32(d, DAY32_EDU_REG_DMA_CMD));
	return -ETIMEDOUT;
}

static int day32_program_dma(struct day32_dev *d, u64 src, u64 dst,
			     u32 count, u32 cmd)
{
	if (!count)
		return -EINVAL;

	d->last_dma_cmd = cmd;
	day32_write64(d, DAY32_EDU_REG_DMA_SRC, src);
	day32_write64(d, DAY32_EDU_REG_DMA_DST, dst);
	day32_write32(d, DAY32_EDU_REG_DMA_COUNT, count);
	day32_write32(d, DAY32_EDU_REG_DMA_CMD, cmd);
	return day32_wait_dma_idle(d);
}

/*
 * ==================== Section 6: 结果记录与重置 ====================
 *
 * day32_reset_run_result()：
 *   重置 day32_dev 中与"最近一次运行"相关的字段。
 *   在 day32_do_run_dma() 开始时调用，确保状态干净。
 *
 * day32_record_mmap_result()：
 *   记录 mmap() 调用后的结果（成功/失败/错误码/length/pgoff）。
 *   在 day32_mmap() 边界检查后调用。
 *
 * 与 Day31 完全相同。
 */
static void day32_reset_run_result(struct day32_dev *d)
{
	d->last_run_ns = 0;
	d->last_run_len = 0;
	d->last_run_seed = 0;
	d->last_run_error = 0;
	d->last_run_ok = 0;
	d->last_irq_delta = 0;
	d->last_dma_cmd = 0;
}

static void day32_record_mmap_result(struct day32_dev *d, bool ok,
				     int err, unsigned long len,
				     unsigned long pgoff)
{
	d->last_mmap_ok = ok ? 1U : 0U;
	d->last_mmap_error = err;
	d->last_mmap_len = (u32)len;
	d->last_mmap_pgoff = (u32)pgoff;
}

/*
 * ==================== Section 7: DMA 运行核心函数 ====================
 *
 * day32_do_run_dma()：
 *   Day32 驱动最核心的函数，触发两段 DMA 往返并记录统计。
 *
 * 【两段 DMA 往返】
 *   Stage 1: RAM(src) → EDU(0x40000)
 *   Stage 2: EDU(0x40000) → RAM(dst)
 *
 * 【ktime_get_ns() 计时】
 *   在 stage1 之前记录 start_ns，
 *   在 stage2 之后记录 end_ns，
 *   last_run_ns = end_ns - start_ns。
 *
 *   这个值代表"内核视角的纯 DMA 耗时"，
 *   不包含用户态 memcpy/memcmp 和 syscall 开销。
 *
 * 【last_irq_delta 验证】
 *   正常情况下，两段 DMA 应该各触发一次 IRQ，共 2 次。
 *   如果 irq_delta != 2，说明有异常（IRQ 丢失或多余）。
 *
 * 与 Day31 完全相同。
 */
static int day32_do_run_dma(struct day32_dev *d, u32 len, u32 seed)
{
	u64 src_dma;
	u64 dst_dma;
	u64 irq_before;
	u64 start_ns;
	u64 end_ns;
	int ret;

	if (!d->dma_virt)
		return -ENODEV;
	if (!len || len > DAY32_DMA_VERIFY_MAX)
		return -EINVAL;

	mutex_lock(&d->op_lock);
	day32_reset_run_result(d);
	d->total_run_calls++;
	d->last_run_len = len;
	d->last_run_seed = seed;

	src_dma = (u64)d->dma_handle + DAY32_DMA_SRC_OFF;
	dst_dma = (u64)d->dma_handle + DAY32_DMA_DST_OFF;
	irq_before = d->irq_count;

	/*
	 * Day32 延续 Day30 的核心设计：
	 *   - buffer 内容由用户态通过 mmap 准备与验证
	 *   - 内核只负责 DMA 发起、等待完成与最小统计
	 *
	 * 这样 bench 结果才能把"用户态内存处理"和"设备参与"分开。
	 */

	start_ns = ktime_get_ns();

	/* Stage 1: RAM → EDU */
	ret = day32_program_dma(d, src_dma, DAY32_EDU_DEVBUF_OFFSET,
				len,
				DAY32_EDU_DMA_CMD_START |
				DAY32_EDU_DMA_CMD_IRQ);
	if (ret) {
		d->last_run_error = ret;
		dev_err(&d->pdev->dev, "run_dma stage1 RAM->EDU failed: %d\n", ret);
		goto out;
	}

	/* Stage 2: EDU → RAM */
	ret = day32_program_dma(d, DAY32_EDU_DEVBUF_OFFSET, dst_dma,
				len,
				DAY32_EDU_DMA_CMD_START |
				DAY32_EDU_DMA_CMD_DIR_TO_RAM |
				DAY32_EDU_DMA_CMD_IRQ);
	if (ret) {
		d->last_run_error = ret;
		dev_err(&d->pdev->dev, "run_dma stage2 EDU->RAM failed: %d\n", ret);
		goto out;
	}

	/*
	 * 【IRQ delta 验证】
	 *   两段 DMA 往返理想情况是触发 2 次 IRQ：
	 *     - Stage 1 完成触发 1 次
	 *     - Stage 2 完成触发 1 次
	 */
	d->last_irq_delta = (u32)(d->irq_count - irq_before);
	d->last_run_ok = 1;
	d->total_run_ok++;

out:
	end_ns = ktime_get_ns();
	d->last_run_ns = end_ns - start_ns;
	if (d->last_run_error)
		d->total_run_fail++;
	mutex_unlock(&d->op_lock);
	return d->last_run_error;
}

/*
 * ==================== Section 8: 设备状态文本导出 ====================
 *
 * day32_build_state_text()：
 *   将 day32_dev 的所有关键状态打包成文本，
 *   供 day32_read()（cat /dev/day32_edu0）输出。
 *
 * 【为什么需要这个函数？】
 *   用户态可以直接 cat 设备节点查看驱动内部状态，
 *   不需要通过 ioctl GET_INFO 也能获取基本信息。
 *
 * 与 Day31 完全相同。
 */
static ssize_t day32_build_state_text(struct day32_dev *d, char *buf, size_t size)
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
			 DAY32_DMA_SRC_OFF,
			 DAY32_DMA_DST_OFF,
			 DAY32_DMA_VERIFY_MAX,
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
 * ==================== Section 9: file_operations 实现 ====================
 *
 * Day32 继续使用与 Day31 完全相同的 file_operations：
 *
 *   day32_open()：从 inode 找到 day32_dev，存入 file->private_data
 *
 *   day32_read()：读取设备状态文本（cat /dev/day32_edu0）
 *                  用于快速验证设备状态，不需要 ioctl
 *
 *   day32_mmap()：将 coherent DMA buffer 映射到用户态
 *                  边界检查：pgoff==0 且 len==PAGE_ALIGN(dma_bytes)
 *                  调用 dma_mmap_coherent() 完成映射
 *
 *   day32_ioctl()：处理三个 ioctl 命令
 *                   GET_INFO / RUN_DMA / GET_RESULT / RESET_STATS
 *
 * 【VMA 标志位】
 *   VM_IO：这是 IO 映射，不是匿名映射
 *   VM_DONTDUMP：从 core dump 排除（敏感设备地址）
 *   VM_DONTEXPAND：不允许 expand（如 mremap）
 */
static int day32_open(struct inode *inode, struct file *file)
{
	struct day32_dev *d = container_of(inode->i_cdev, struct day32_dev, cdev);

	file->private_data = d;
	return 0;
}

static ssize_t day32_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct day32_dev *d = file->private_data;
	char kbuf[384];
	ssize_t len;

	if (!d || !d->bar0)
		return -ENODEV;

	len = day32_build_state_text(d, kbuf, sizeof(kbuf));
	return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

static int day32_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct day32_dev *d = file->private_data;
	unsigned long len = vma->vm_end - vma->vm_start;
	unsigned long map_bytes = PAGE_ALIGN(d->dma_bytes);
	int ret;

	if (!d || !d->dma_virt)
		return -ENODEV;

	/*
	 * Day32 继续只支持"整页映射 + offset=0"：
	 *
	 * 【为什么 pgoff 必须为 0？】
	 *   coherent DMA buffer 只有 4KB，
	 *   我们只映射这一块，所以 pgoff=0。
	 *   如果 pgoff!=0，说明用户想要映射别的区域，驱动无法支持。
	 *
	 * 【为什么 len 必须等于 PAGE_ALIGN(dma_bytes)？】
	 *   mmap() 会按页对齐分配 VMA，
	 *   如果 len 不是页对齐的，kernel 会自动 round up。
	 *   我们期望 len == 4096（4KB，一页）。
	 *
	 * 【mmap 边界检查总结】
	 *   允许：pgoff==0 && len==4096
	 *   拒绝：pgoff!=0 或 len!=4096
	 */
	if (vma->vm_pgoff != 0) {
		day32_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
		dev_err(&d->pdev->dev,
			"mmap rejected: pgoff=%lu expected=0\n",
			vma->vm_pgoff);
		return -EINVAL;
	}

	if (len != map_bytes) {
		day32_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
		dev_err(&d->pdev->dev,
			"mmap rejected: len=%lu expected=%lu\n",
			len, map_bytes);
		return -EINVAL;
	}

	/*
	 * 设置 VMA 标志：
	 *   VM_IO：这是一个设备 IO 映射，不是匿名内存
	 *   VM_DONTDUMP：从 core dump 排除（避免泄露设备地址）
	 *   VM_DONTEXPAND：不允许通过 mremap 扩展（我们是固定大小）
	 */
	vma->vm_flags |= VM_IO | VM_DONTEXPAND;

	/*
	 * 【dma_mmap_coherent 的关键作用】
	 *
	 * 这是 Day30-32 的核心函数：
	 *   1. 让用户态直接访问 coherent DMA buffer
	 *   2. 实现零拷贝：用户态 memcpy 直接操作 DMA buffer
	 *   3. 绕过内核页缓存，减少一次拷贝
	 *
	 * 参数说明：
	 *   &d->pdev->dev：关联的 PCI 设备
	 *   vma：用户态 VMA（内核会在里面建立物理页映射）
	 *   d->dma_virt：CPU 侧虚拟地址（kmalloc/vmalloc）
	 *   d->dma_handle：设备侧 DMA 地址（总线地址）
	 *   d->dma_bytes：映射大小（4096）
	 */
	ret = dma_mmap_coherent(&d->pdev->dev, vma,
				d->dma_virt, d->dma_handle, d->dma_bytes);
	day32_record_mmap_result(d, ret == 0, ret, len, vma->vm_pgoff);
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
 * ==================== Section 10: ioctl 命令处理 ====================
 *
 * Day32 支持四个 ioctl 命令：
 *
 *   DAY32_IOC_GET_INFO：
 *     返回 day32_info 结构体，包含所有驱动状态。
 *     用户态 bench 工具用它获取 map_bytes/src_off/dst_off。
 *
 *   DAY32_IOC_RUN_DMA：
 *     传入 day32_run_req{len, pattern_seed}，
 *     内核执行 DMA 往返，返回 0 或错误码。
 *
 *   DAY32_IOC_GET_RESULT：
 *     返回 day32_run_result{total_run_*, last_run_*, ...}。
 *     用户态 bench 工具用它查询统计信息。
 *
 *   DAY32_IOC_RESET_STATS：
 *     重置所有统计计数器（irq_count / total_run_* 等）。
 *     用于开始一次新的 bench 迭代。
 *
 * 【ioctl 为什么用 copy_to_user/copy_from_user？】
 *   ioctl 第三个参数是用户态指针（unsigned long arg），
 *   内核不能直接解引用，必须用 copy_*_user 安全访问。
 */
static long day32_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct day32_dev *d = file->private_data;

	switch (cmd) {
	case DAY32_IOC_GET_INFO: {
		struct day32_info info = {
			.tool_api_version = DAY32_TOOL_API_VERSION,
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
			.src_off = DAY32_DMA_SRC_OFF,
			.dst_off = DAY32_DMA_DST_OFF,
			.max_verify_len = DAY32_DMA_VERIFY_MAX,
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
	case DAY32_IOC_RUN_DMA: {
		struct day32_run_req req;

		if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
			return -EFAULT;
		return day32_do_run_dma(d, req.len, req.pattern_seed);
	}
	case DAY32_IOC_GET_RESULT: {
		struct day32_run_result res = {
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
	case DAY32_IOC_RESET_STATS:
		mutex_lock(&d->op_lock);
		d->irq_count = 0;
		d->last_irq_status = 0;
		d->last_ack_value = 0;
		d->total_run_calls = 0;
		d->total_run_ok = 0;
		d->total_run_fail = 0;
		day32_reset_run_result(d);
		day32_record_mmap_result(d, false, 0, 0, 0);
		mutex_unlock(&d->op_lock);
		return 0;
	default:
		return -ENOTTY;
	}
}

static const struct file_operations day32_fops = {
	.owner          = THIS_MODULE,
	.open           = day32_open,
	.read           = day32_read,
	.mmap           = day32_mmap,
	.unlocked_ioctl = day32_ioctl,
	.llseek         = no_llseek,
};

/*
 * ==================== Section 11: 字符设备注册与销毁 ====================
 *
 * day32_setup_chrdev()：
 *   1. 原子递增获取次设备号
 *   2. 初始化 cdev 并绑定 file_operations
 *   3. 添加 cdev 到系统
 *   4. 创建 sysfs 设备节点（/sys/class/day32_edu/day32_eduN）
 *
 * day32_destroy_chrdev()：
 *   逆向销毁（device_destroy → cdev_del）
 *
 * 与 Day31 完全相同。
 */
static int day32_setup_chrdev(struct day32_dev *d)
{
	int minor;
	int ret;

	minor = atomic_fetch_add(1, &g_day32_minor);
	d->devt = MKDEV(MAJOR(g_day32_base_dev), minor);

	cdev_init(&d->cdev, &day32_fops);
	d->cdev.owner = THIS_MODULE;

	ret = cdev_add(&d->cdev, d->devt, 1);
	if (ret)
		return ret;

	d->device = device_create(g_day32_class, &d->pdev->dev, d->devt, NULL,
				  DAY32_DEV_NAME_FMT, minor);
	if (IS_ERR(d->device)) {
		ret = PTR_ERR(d->device);
		d->device = NULL;
		cdev_del(&d->cdev);
		return ret;
	}
	return 0;
}

static void day32_destroy_chrdev(struct day32_dev *d)
{
	if (d->device)
		device_destroy(g_day32_class, d->devt);
	cdev_del(&d->cdev);
}

/*
 * ==================== Section 12: PCI Probe ====================
 *
 * day32_probe()：
 *   PCI 设备初始化完整流程。
 *
 * 【初始化顺序】
 *   1. kzalloc 分配 day32_dev
 *   2. pci_enable_device 启用 PCI 设备
 *   3. dma_set_mask_and_coherent 设置 DMA mask
 *   4. pci_request_regions 请求 BAR0 资源
 *   5. pci_set_master 启用总线主设备
 *   6. pci_iomap 映射 BAR0 MMIO
 *   7. 读取 ID/LIVENESS 验证设备
 *   8. dma_alloc_coherent 分配 DMA buffer
 *   9. pci_alloc_irq_vectors 分配 MSI 中断向量
 *   10. request_irq 注册中断处理函数
 *   11. day32_setup_chrdev 注册字符设备
 *
 * 【资源释放顺序（err_* 标签）】
 *   err_irq：free_irq / pci_free_irq_vectors
 *   err_dma：dma_free_coherent
 *   err_iounmap：pci_iounmap
 *   err_regions：pci_release_regions
 *   err_disable：pci_disable_device
 *   err_free：kfree
 *
 * 与 Day31 完全相同。
 */
static int day32_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct day32_dev *d;
	u32 ident;
	u32 live;
	int ret;

	dev_info(&pdev->dev, "probe enter: %04x:%04x\n", pdev->vendor, pdev->device);

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	d->pdev = pdev;
	d->dma_mask_bits = dma_mask_bits;
	d->dma_bytes = DAY32_DMA_BYTES;
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

	ret = pci_request_regions(pdev, DAY32_DRV_NAME);
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

	ident = day32_read32(d, DAY32_EDU_REG_IDENTITY);
	live = day32_read32(d, DAY32_EDU_REG_LIVENESS);
	dev_info(&pdev->dev, "ident=0x%08x liveness=0x%08x\n", ident, live);

	/*
	 * 【coherent DMA buffer 分配】
	 *
	 * dma_alloc_coherent() 返回两个地址：
	 *   - d->dma_virt：CPU 侧虚拟地址（用于 memset/memcpy）
	 *   - d->dma_handle：设备侧 DMA 地址（用于 DMA 寄存器编程）
	 *
	 * 这个 buffer 会被 mmap() 暴露给用户态，
	 * 用户态可以直接读写这个 buffer（零拷贝）。
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

	ret = request_irq(d->irq_vector, day32_irq_handler, 0, DAY32_DRV_NAME, d);
	if (ret) {
		dev_err(&pdev->dev, "request_irq(%u) failed: %d\n",
			d->irq_vector, ret);
		goto err_irq_vectors;
	}
	dev_info(&pdev->dev, "MSI vector=%u enabled=%u\n",
		 d->irq_vector, !!pdev->msi_enabled);

	ret = day32_setup_chrdev(d);
	if (ret) {
		dev_err(&pdev->dev, "day32_setup_chrdev failed: %d\n", ret);
		goto err_irq;
	}

	day32_reset_run_result(d);
	day32_record_mmap_result(d, false, 0, 0, 0);
	dev_info(&pdev->dev, "probe success\n");
	return 0;

err_irq:
	free_irq(d->irq_vector, d);
err_irq_vectors:
	pci_free_irq_vectors(pdev);
err_dma:
	dma_free_coherent(&pdev->dev, d->dma_bytes, d->dma_virt, d->dma_handle);
err_iounmap:
	pci_iomap(pdev, d->bar0);
err_regions:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
err_free:
	kfree(d);
	return ret;
}

/*
 * ==================== Section 13: PCI Remove ====================
 *
 * day32_remove()：
 *   PCI 设备移除时的资源释放。
 *   完全对称于 day32_probe() 的分配顺序。
 *
 * 与 Day31 完全相同。
 */
static void day32_remove(struct pci_dev *pdev)
{
	struct day32_dev *d = pci_get_drvdata(pdev);

	if (!d)
		return;

	dev_info(&pdev->dev, "remove enter\n");
	day32_destroy_chrdev(d);
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
 * ==================== Section 14: PCI ID Table与驱动注册 ====================
 *
 * PCI ID 表只匹配 QEMU EDU 设备：
 *   Vendor=0x1234, Device=0x11e8
 *
 * pci_driver 注册：
 *   name：驱动名（day32_edu_perf）
 *   id_table：匹配的设备列表
 *   probe/remove：设备插入/移除回调
 *
 * 模块初始化：
 *   1. alloc_chrdev_region 分配主设备号
 *   2. class_create 创建 sysfs 类
 *   3. pci_register_driver 注册 PCI 驱动
 *
 * 模块退出：
 *   逆向释放所有资源。
 */
static const struct pci_device_id day32_pci_ids[] = {
	{ PCI_DEVICE(DAY32_EDU_VENDOR_ID, DAY32_EDU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, day32_pci_ids);

static struct pci_driver day32_pci_driver = {
	.name = DAY32_DRV_NAME,
	.id_table = day32_pci_ids,
	.probe = day32_probe,
	.remove = day32_remove,
};

static int __init day32_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&g_day32_base_dev, 0, 256, DAY32_DRV_NAME);
	if (ret)
		return ret;

	g_day32_class = class_create(THIS_MODULE, DAY32_CLASS_NAME);
	if (IS_ERR(g_day32_class)) {
		ret = PTR_ERR(g_day32_class);
		unregister_chrdev_region(&g_day32_base_dev, 256);
		return ret;
	}

	ret = pci_register_driver(&day32_pci_driver);
	if (ret) {
		class_destroy(g_day32_class);
		unregister_chrdev_region(g_day32_base_dev, 256);
		return ret;
	}

	pr_info(DAY32_DRV_NAME ": init ok\n");
	return 0;
}

static void __exit day32_exit(void)
{
	pci_unregister_driver(&day32_pci_driver);
	class_destroy(g_day32_class);
	unregister_chrdev_region(g_day32_base_dev, 256);
	pr_info(DAY32_DRV_NAME ": exit ok\n");
}

module_init(day32_init);
module_exit(day32_exit);

MODULE_AUTHOR("Richer Wong");
MODULE_DESCRIPTION("Day32 QEMU EDU perf driver");
MODULE_LICENSE("GPL");

/*
 * ==================== Appendix A: Day32 优化点详解 ====================
 *
 * Day32 的优化点是用户态 mmap 复用，不是驱动改动。
 *
 * 【baseline 路径：每轮重新 mmap】
 *   for iter:
 *       get_info()         ← syscall
 *       mmap()             ← syscall + VMA 创建
 *       memcpy()           ← 用户态内存操作
 *       memcmp()          ← 用户态内存操作
 *       munmap()           ← syscall + VMA 销毁
 *
 * 【optimized 路径：提前 mmap】
 *   get_info()             ← 只做一次
 *   mmap()                 ← 只做一次
 *   for iter:
 *       memcpy()           ← 只有这个在热路径
 *       memcmp()           ← 只有这个在热路径
 *   munmap()               ← 只做一次
 *
 * 【优化效果】
 *   - avg_latency_gain_pct: 99.65%（延迟降低 99.65%）
 *   - throughput_gain_pct: 24826%（吞吐量提升 248 倍）
 *
 * 【为什么 mmap+munmap 成本高？】
 *   1. 两次 syscall（用户态/内核态切换）
 *   2. 两个 VMA 创建/销毁（内核数据结构操作）
 *   3. 页表更新（如果触发 page fault）
 *   4. 内核锁竞争（VMA 链表/红黑树修改）
 *
 *   当 len=256 字节时，memcpy 成本 ≈ 0.05 微秒，
 *   mmap+munmap 成本 ≈ 280 微秒，
 *   所以搬出 mmap 能带来 99%+ 的性能提升。
 */

/*
 * ==================== Appendix B: perf 工具链在 Day32 的用法 ====================
 *
 * Day32 使用宿主端 perf 工具采集热点：
 *
 * 【perf stat - 统计计数器】
 *   perf stat -d -o output.stat.txt ./bench-mmap
 *   看到：cycles, instructions, cache-misses, branch-mispredict
 *
 * 【perf record - 采样录制】
 *   perf record -F 49 -g -o output.data ./bench-mmap
 *   -F 49：每秒 49 次采样（避免干扰被测程序）
 *   -g：记录调用链
 *
 * 【perf report - 报告生成】
 *   perf report --stdio -i output.data
 *   显示热点函数，按 CPU 占用排序
 *
 * 【baseline vs optimized 的 perf 预期差异】
 *   baseline perf 报告：
 *     - mmap/munmap syscall 占比高
 *     - VMA 相关函数（mlock_lock, vm_area_struct 操作）占比高
 *
 *   optimized perf 报告：
 *     - memcpy/memcmp 占比高
 *     - mmap/munmap 热点消失（只调用一次，不在热路径）
 */

/*
 * ==================== Appendix C: Day30-32 演进关系 ====================
 *
 * Day30：mmap 零拷贝链路
 *   - 驱动提供 mmap 接口
 *   - 用户态直接访问 DMA buffer
 *   - 验证零拷贝能工作
 *
 * Day31：mmap 基准测试
 *   - 建立三条路径的性能基准
 *   - 测量 avg/p50/p95/p99/throughput
 *   - 纯测量，不做优化
 *
 * Day32：perf 热点分析与优化
 *   - 用 perf stat/record/report 找到热点
 *   - 识别 mmap+munmap 为热点
 *   - 用 optimized 路径验证优化效果
 *   - 产出可复读的前后对比证据
 */
