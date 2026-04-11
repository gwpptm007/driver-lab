/* SPDX-License-Identifier: GPL-2.0 */
/*
 * day34_edu_stability.h - Day34 QEMU EDU 稳定性测试驱动头文件
 *
 * ==================== 头文件作用 ====================
 *
 * 定义 Day34 驱动所需的常量、结构体、寄存器布局。
 *
 * 与 Day33 头文件相比，Day34 主要变化：
 *   - 模块参数名从 trace_verbose 改为 stability_verbose
 *   - 新增 irq_vectors_allocated 和 irq_requested 标志
 *   - 新增 mmap 结果记录字段（last_mmap_ok/error/len/pgoff）
 *   - 其他所有常量、结构体、寄存器布局与 Day33 基本相同
 *
 * Day34 的核心在稳定性测试设计，不在驱动本身。
 *
 * ==================== 头文件内容 ====================
 *
 *  第1部分：头文件依赖
 *  第2部分：模块和设备命名常量
 *  第3部分：EDU 设备 PCI ID
 *  第4部分：EDU 寄存器布局（MMIO + DMA）
 *  第5部分：DMA 相关常量
 *  第6部分：day34_dev 结构体
 *  第7部分：稳定性测试架构图
 */

#ifndef DAY34_EDU_STABILITY_H
#define DAY34_EDU_STABILITY_H

/*
 * ==================== 第1部分：头文件依赖 ====================
 *
 * Day34 新增：
 *   linux/types.h → bool 类型（用于 irq_vectors_allocated/irq_requested）
 *
 * 其他头文件与 Day33 完全相同：
 *   linux/cdev.h    → struct cdev（字符设备）
 *   linux/device.h → struct class, struct device（设备模型）
 *   linux/mutex.h  → struct mutex（操作锁）
 *   linux/pci.h     → struct pci_dev（PCI 设备）
 *   linux/spinlock.h → struct spinlock（中断自旋锁）
 *
 * Day34 不需要额外的头文件。
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/types.h>

/*
 * ==================== 第2部分：模块和设备命名常量 ====================
 *
 * 与 Day33 基本相同，只是名称反映 Day34 稳定性测试主题。
 */
#define DAY34_DRV_NAME              "day34_edu_stability"  /* 驱动名 */
#define DAY34_CLASS_NAME            "day34_edu"            /* sysfs 类名 */
#define DAY34_DEV_NAME_FMT          "day34_edu%d"         /* 设备节点名格式 */

/*
 * ==================== 第3部分：EDU 设备 PCI ID ====================
 *
 * 与 Day33 完全相同：Vendor=1234, Device=11e8
 */
#define DAY34_EDU_VENDOR_ID         0x1234
#define DAY34_EDU_DEVICE_ID          0x11e8

/*
 * ==================== 第4部分：EDU 寄存器布局 ====================
 *
 * 与 Day33 完全相同：
 *   MMIO：0x24 IRQ_STATUS, 0x64 IRQ_ACK
 *   DMA：0x80 DMA_SRC, 0x88 DMA_DST, 0x90 DMA_COUNT, 0x98 DMA_CMD
 *
 * 注意：Day34 移除了 ID (0x00) 和 LIVENESS (0x04) 寄存器定义，
 * 因为稳定性测试不读取这些寄存器（不影响功能）。
 */

/* MMIO 寄存器 */
#define DAY34_EDU_REG_IRQ_STATUS    0x24
#define DAY34_EDU_REG_IRQ_ACK       0x64

/* DMA 寄存器 */
#define DAY34_EDU_REG_DMA_SRC       0x80
#define DAY34_EDU_REG_DMA_DST       0x88
#define DAY34_EDU_REG_DMA_COUNT     0x90
#define DAY34_EDU_REG_DMA_CMD       0x98

/*
 * ==================== 第5部分：DMA 相关常量 ====================
 *
 * 与 Day33 完全相同：
 *   DEVBUF_OFFSET = 0x40000
 *   DMA 命令位：START=0x01, DIR_TO_RAM=0x02, IRQ=0x04
 *   Buffer：4096 bytes，src 偏移 0，dst 偏移 2048
 */
#define DAY34_EDU_DEVBUF_OFFSET        0x40000ULL
#define DAY34_EDU_DMA_CMD_START        0x01
#define DAY34_EDU_DMA_CMD_DIR_TO_RAM   0x02
#define DAY34_EDU_DMA_CMD_IRQ          0x04

#define DAY34_DMA_MASK_BITS_DEFAULT    32
#define DAY34_DMA_BYTES                4096
#define DAY34_DMA_SRC_OFF              0
#define DAY34_DMA_DST_OFF              2048
#define DAY34_DMA_VERIFY_MAX           2048

/*
 * ==================== 第6部分：day34_dev 结构体 ====================
 *
 * 【与 day33_dev 的区别】
 *
 *   1. 新增 irq_vectors_allocated 标志
 *      → 标记 MSI/LEGACY 向量是否已申请
 *
 *   2. 新增 irq_requested 标志
 *      → 标记 IRQ handler 是否已注册
 *
 *   3. 新增 last_mmap_* 字段
 *      → 记录最近一次 mmap 尝试的结果（用于错误注入验证）
 *
 *   4. 移除了 ID/LIVENESS 相关字段（稳定性测试不需要）
 *
 * 【双标志设计】
 *
 *   irq_vectors_allocated + irq_requested 两个布尔标志，
 *   确保在以下场景都能正确回滚：
 *
 *   场景A：probe 失败回滚
 *     - pci_alloc_irq_vectors() 成功 → irq_vectors_allocated = true
 *     - request_irq() 失败 → 只释放 IRQ 向量，不释放 DMA
 *
 *   场景B：remove 被调用但 probe 部分失败
 *     - irq_vectors_allocated 和 irq_requested 可能只有一个为 true
 *     - 两个标志确保只释放已申请的资源
 */
struct day34_dev {
    /*
     * ─────────────── PCI 资源 ───────────────
     */
    struct pci_dev *pdev;
    void __iomem *bar0;
    resource_size_t bar0_start;
    resource_size_t bar0_len;

    /*
     * ─────────────── 中断资源 ───────────────
     *
     * 【新增标志】
     *
     * irq_vectors_allocated：MSI/LEGACY 向量是否已申请
     *   - true：pci_alloc_irq_vectors() 成功
     *   - false：未申请，或已释放
     *
     * irq_requested：IRQ handler 是否已注册
     *   - true：request_irq() 成功
     *   - false：未注册，或已释放
     *
     * 为什么需要两个？
     *   - pci_alloc_irq_vectors() 和 request_irq() 是两个独立步骤
     *   - 任何一个失败都需要单独回滚
     *   - remove() 中需要根据这两个标志决定是否释放
     */
    unsigned int irq_vector;
    bool irq_vectors_allocated;
    bool irq_requested;
    u64 irq_count;
    u32 last_irq_status;
    u32 last_ack_value;
    spinlock_t irq_lock;

    /*
     * ─────────────── DMA 资源 ───────────────
     */
    void *dma_virt;
    dma_addr_t dma_handle;
    size_t dma_bytes;
    u32 dma_mask_bits;

    /*
     * ─────────────── Bench 统计 ───────────────
     *
     * 【与 Day33 完全相同的字段】
     *   total_run_calls：RUN_DMA 累计调用次数
     *   total_run_ok：累计成功次数
     *   total_run_fail：累计失败次数
     *   last_run_ns：最近一次 DMA 耗时（纳秒）
     */
    u64 total_run_calls;    /* RUN_DMA 累计调用次数 */
    u64 total_run_ok;       /* 累计成功次数 */
    u64 total_run_fail;     /* 累计失败次数 */
    u64 last_run_ns;        /* 最近一次 DMA 耗时（纳秒）*/

    /*
     * ─────────────── 最近一次运行结果 ───────────────
     */
    u32 last_run_len;
    u32 last_run_seed;
    s32 last_run_error;
    u32 last_run_ok;
    u32 last_irq_delta;
    u32 last_dma_cmd;

    /*
     * ─────────────── mmap 结果 ───────────────
     *
     * 【Day34 新增的字段】
     *
     * 记录最近一次 mmap 尝试的结果，用于：
     *   1. 错误注入验证（mmap offset 错误）
     *   2. 用户态调试（查看 mmap 是否成功）
     *   3. 回归测试（确认 mmap 路径稳定）
     *
     * 为什么需要记录？
     *   mmap() 的返回值有限（只有成功/失败）
     *   用户态需要知道失败的具体原因（errno）和参数（len/pgoff）
     */
    u32 last_mmap_ok;       /* mmap 是否成功（1=成功，0=失败）*/
    s32 last_mmap_error;    /* errno（负数，如 -EINVAL）*/
    u32 last_mmap_len;      /* mmap 请求的 length */
    u32 last_mmap_pgoff;    /* mmap 请求的 page offset */

    /*
     * ─────────────── 操作锁 ───────────────
     */
    struct mutex op_lock;

    /*
     * ─────────────── 字符设备资源 ───────────────
     */
    dev_t devt;
    struct cdev cdev;
    struct device *device;
};

/*
 * ==================== 第7部分：稳定性测试架构图 ====================
 *
 * 【Day34 稳定性测试三大场景】
 *
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │                    Day34 稳定性测试架构                               │
 * ├─────────────────────────────────────────────────────────────────────┤
 * │                                                                      │
 * │  场景1：并发压测                                                      │
 * │  ────────────────────                                               │
 * │  stress-mmap x3        stress-ioctl x1                              │
 * │      ↓                      ↓                                      │
 * │  flock(LOCK_EX)        ioctl(RUN_DMA)                               │
 * │      ↓                      ↓                                      │
 * │  mmap()                    ↓                                       │
 * │      ↓                      ↓                                      │
 * │  RUN_DMA (via ioctl)       ↓                                       │
 * │      ↓                      ↓                                      │
 * │  memcmp(src, dst)          ↓                                       │
 * │      ↓                      ↓                                      │
 * │  munmap()                   ↓                                      │
 * │      ↓                      ↓                                      │
 * │  flock(LOCK_UN)             ↓                                      │
 * │                                                                      │
 * │  共享资源：                                                       │
 * │    - /dev/day34_edu0（设备节点）                                      │
 * │    - DMA buffer（src @ 0, dst @ 2048）                               │
 * │    - op_lock（驱动统计）                                             │
 * │                                                                      │
 * │  用户态协调：flock() 保护自己的缓冲区                                  │
 * │                                                                      │
 * ├─────────────────────────────────────────────────────────────────────┤
 * │                                                                      │
 * │  场景2：模块循环（1000 次）                                          │
 * │  ──────────────────────────────                                     │
 * │  for i in $(seq 1 1000); do                                         │
 * │      rmmod day34_edu_stability                                       │
     │      insmod /root/day34_edu_stability.ko                           │
     │  done                                                              │
     │                                                                      │
     │  关键：                                                             │
     │    free_irq() → pci_free_irq_vectors() → dma_free_coherent()       │
     │                                                                      │
     ├─────────────────────────────────────────────────────────────────────┤
     │                                                                      │
     │  场景3：错误注入                                                    │
     │  ──────────────                                                    │
     │  测试1：len > 2048                                                 │
     │      ioctl(RUN_DMA, {len=3000})                                    │
     │      → 驱动返回 -EINVAL                                            │
     │                                                                      │
     │  测试2：pgoff != 0                                                 │
     │      mmap(fd, 4096, PROT_*, MAP_SHARED, 4096)                      │
     │      → 驱动返回 -EINVAL                                            │
     │                                                                      │
     └─────────────────────────────────────────────────────────────────────┘
 *
 * 【资源释放顺序（与申请顺序相反）】
 *
 *   remove() 中的释放顺序：
 *
 *   1. device_destroy()     → 销毁设备节点
 *   2. cdev_del()          → 删除字符设备
 *   3. free_irq()          → 【先】释放 IRQ handler（关键！）
 *   4. pci_free_irq_vectors() → 【后】释放 MSI vectors
 *   5. dma_free_coherent() → 释放 DMA buffer
 *
 *   为什么顺序不能错？
 *     - 如果先 pci_free_irq_vectors() 再 free_irq()，
 *     - MSI vector 已经被释放，但 IRQ handler 仍然注册在上面，
 *     - 内核 IRQ layer 在处理中断时可能访问已释放的 vector，
 *     - 触发 BUG in free_msi_irqs()
 *
 * 【双标志回滚逻辑】
 *
 *   if (d->irq_requested) {
 *       free_irq(d->irq_vector, d);
 *       d->irq_requested = false;
 *   }
 *   if (d->irq_vectors_allocated) {
 *       pci_free_irq_vectors(pdev);
 *       d->irq_vectors_allocated = false;
 *   }
 *
 *   确保：
 *     - 只释放已申请的资源
 *     - 两个标志独立判断，不需要特定顺序
 */

/*
 * 【模块参数：stability_verbose】
 *
 * Day34 新增 stability_verbose 模块参数：
 *
 *   insmod day34_edu_stability.ko              # 默认，关闭热路径日志
 *   insmod day34_edu_stability.ko stability_verbose=1  # 开启，用于调试
 *
 * 为什么要关闭？
 *   如果开启，每次 IRQ 都会 dev_info() 打印，
 *   1000 次模块循环 + 并发压测会产生海量输出，拖慢测试。
 *
 * 为什么叫 stability_verbose？
 *   Day34 主题是稳定性测试，这个名称反映主题。
 */

/*
 * 【Day34 与 Day33 的区别】
 *
 *   Day33：用 ftrace function_graph 看清调用路径
 *   Day34：用稳定性测试验证长期可靠性
 *
 *   Day33 做了 ftrace tracer 配置
 *   Day34 不做新功能，只是验证已有功能在压力下稳定
 *
 *   Day33：prove it works（证明功能正确）
 *   Day34：prove it keeps working（证明长期可靠）
 */

/*
 * 【验收标准】
 *
 * Day34 必须满足：
 *   1. mmap-verify 通过（主数据路径可用）
 *   2. concurrent-stress 通过（所有 worker rc=0）
 *   3. module-loop 完成 1000 次循环（failed_loops=0）
 *   4. fault-invalid-len 返回 -EINVAL
 *   5. fault-mmap-offset 返回 -EINVAL
 *   6. guest 正常结束，无 BUG/Oops/panic
 */

#endif /* DAY34_EDU_STABILITY_H */
