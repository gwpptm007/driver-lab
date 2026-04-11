/* SPDX-License-Identifier: GPL-2.0 */
/*
 * day31_edu_bench.h - Day31 QEMU EDU bench 驱动头文件
 *
 * ==================== 头文件作用 ====================
 *
 * 定义 Day31 驱动所需的常量、结构体、寄存器布局。
 *
 * 与 Day30 头文件相比，Day31 新增：
 *   - bench 统计相关常量（ioctl magic 'B'）
 *   - 结构体新增 total_run_calls/ok/fail/last_run_ns 字段
 *   - 注释反映 bench 主题
 *
 * ==================== 头文件内容 ====================
 *
 *  1. 模块和设备的命名常量
 *  2. EDU 寄存器布局（MMIO + DMA）
 *  3. DMA 相关常量
 *  4. day31_dev 结构体
 */

#ifndef DAY31_EDU_BENCH_H
#define DAY31_EDU_BENCH_H

/*
 * ==================== 头文件依赖 ====================
 *
 * 与 Day30 完全相同：
 *   linux/cdev.h    → struct cdev（字符设备）
 *   linux/device.h → struct class, struct device（设备模型）
 *   linux/mutex.h  → struct mutex（操作锁）
 *   linux/pci.h     → struct pci_dev（PCI 设备）
 *   linux/spinlock.h → struct spinlock（中断自旋锁）
 *
 * Day31 不需要额外的头文件：
 *   ktime_get_ns() 在 .c 文件中 #include <linux/ktime.h>
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

/*
 * ==================== 第1部分：模块和设备命名常量 ====================
 *
 * 与 Day30 基本相同，只是名称反映 Day31 bench 主题。
 */
#define DAY31_DRV_NAME              "day31_edu_bench"  /* 驱动名 */
#define DAY31_CLASS_NAME            "day31_edu"        /* sysfs 类名 */
#define DAY31_DEV_NAME_FMT          "day31_edu%d"      /* 设备节点名格式 */

/*
 * ==================== 第2部分：EDU 设备 PCI ID ====================
 *
 * 与 Day30 完全相同：Vendor=1234, Device=11e8
 */
#define DAY31_EDU_VENDOR_ID         0x1234
#define DAY31_EDU_DEVICE_ID          0x11e8

/*
 * ==================== 第3部分：EDU 寄存器布局 ====================
 *
 * 与 Day30 完全相同：
 *   MMIO：0x00 ID, 0x04 LIVENESS, 0x24 IRQ_STATUS, 0x64 IRQ_ACK
 *   DMA：0x80 DMA_SRC, 0x88 DMA_DST, 0x90 DMA_COUNT, 0x98 DMA_CMD
 */

/* MMIO 寄存器 */
#define DAY31_EDU_REG_IDENTITY      0x00
#define DAY31_EDU_REG_LIVENESS      0x04
#define DAY31_EDU_REG_IRQ_STATUS    0x24
#define DAY31_EDU_REG_IRQ_ACK       0x64

/* DMA 寄存器 */
#define DAY31_EDU_REG_DMA_SRC       0x80
#define DAY31_EDU_REG_DMA_DST       0x88
#define DAY31_EDU_REG_DMA_COUNT     0x90
#define DAY31_EDU_REG_DMA_CMD       0x98

/*
 * ==================== 第4部分：DMA 相关常量 ====================
 *
 * 与 Day30 完全相同：
 *   DEVBUF_OFFSET = 0x40000
 *   DMA 命令位：START=0x01, DIR_TO_RAM=0x02, IRQ=0x04
 *   Buffer：4096 bytes，src 偏移 0，dst 偏移 2048
 */
#define DAY31_EDU_DEVBUF_OFFSET        0x40000ULL
#define DAY31_EDU_DMA_CMD_START        0x01
#define DAY31_EDU_DMA_CMD_DIR_TO_RAM   0x02
#define DAY31_EDU_DMA_CMD_IRQ          0x04

#define DAY31_DMA_MASK_BITS_DEFAULT    32
#define DAY31_DMA_BYTES                4096
#define DAY31_DMA_SRC_OFF              0
#define DAY31_DMA_DST_OFF              2048
#define DAY31_DMA_VERIFY_MAX           2048

/*
 * ==================== 第5部分：day31_dev 结构体 ====================
 *
 * 【与 day30_dev 的区别】
 *
 * 新增 bench 统计字段：
 *   - total_run_calls：RUN_DMA 累计调用次数
 *   - total_run_ok：累计成功次数
 *   - total_run_fail：累计失败次数
 *   - last_run_ns：最近一次 DMA 运行的纳秒级耗时
 *
 * 这些字段用于：
 *   - 用户态 bench 工具可以查询累计成功率
 *   - 内核可以记录精确的 DMA 耗时
 *   - 配合 last_run_ns 可以分析性能瓶颈在 syscall 还是 DMA
 */
struct day31_dev {
    /*
     * ─────────────── PCI 资源 ───────────────
     */
    struct pci_dev *pdev;
    void __iomem *bar0;
    resource_size_t bar0_start;
    resource_size_t bar0_len;

    /*
     * ─────────────── 中断资源 ───────────────
     */
    unsigned int irq_vector;
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
     * ─────────────── Bench 统计（新增）──────────────
     *
     * 【total_run_calls / ok / fail】
     *   累计计数器，用于：
     *     - bench 结束后看总成功率
     *     - 检测是否有偶发失败
     *
     * 【last_run_ns】
     *   最近一次 DMA 运行的耗时（纳秒）
     *   由 ktime_get_ns() 在 day31_do_run_dma() 中计算
     *
     * 为什么需要 last_run_ns？
     *   - 用户态计时包含 memcpy/ioctl syscall 等开销
     *   - last_run_ns 只包含纯 DMA 耗时
     *   - 两者对比可以分析性能瓶颈
     *
     * 典型值：
     *   - 用户态 bench-dma：avg_us ≈ 200ms
     *   - 内核 last_run_ns：≈ 200ms（200,000,000 ns）
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
     */
    u32 last_mmap_ok;
    s32 last_mmap_error;
    u32 last_mmap_len;
    u32 last_mmap_pgoff;

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
 * 【Day31 三条 Bench 路径】
 *
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │                     Day31 Bench 三条路径                              │
 * ├─────────────────────────────────────────────────────────────────────┤
 * │                                                                      │
 * │  路径 A：ioctl 控制路径                                              │
 * │  ─────────────────────                                              │
 * │  bench-ioctl：ioctl(GET_INFO)                                        │
 * │  测量：用户态 → 内核态 → 返回 的控制开销                              │
 * │  典型结果：avg_us ≈ 16μs                                             │
 * │                                                                      │
 * │  路径 B：mmap 用户态路径                                             │
 * │  ─────────────────────                                              │
 * │  bench-mmap：memcpy + memcmp（纯用户态，无内核/设备参与）              │
 * │  测量：直接内存访问速度                                               │
 * │  典型结果：avg_us ≈ 0.5μs, throughput ≈ 887MB/s                      │
 * │                                                                      │
 * │  路径 C：DMA 端到端路径                                             │
 * │  ─────────────────────                                              │
 * │  bench-dma：fill_pattern + memset + ioctl(RUN_DMA) + memcmp          │
 * │  测量：零拷贝 mmap + EDU DMA 往返的完整路径                          │
 * │  典型结果：avg_us ≈ 200ms（QEMU EDU 软件模拟，较慢）                  │
 * │                                                                      │
 * └─────────────────────────────────────────────────────────────────────┘
 *
 * 【last_run_ns 的作用】
 *
 *   用户态计时                        内核 last_run_ns
 *   bench-dma avg_us                 纯 DMA 耗时
 *   = 200ms                          = 200ms（假设）
 *
 *   如果 avg_us >> last_run_ns → 瓶颈在 syscall/用户态
 *   如果 avg_us ≈ last_run_ns → 瓶颈在 DMA 设备
 */

/*
 * 【模块参数】
 *
 * Day31 新增 bench_verbose 模块参数：
 *
 *   insmod day31_edu_bench.ko bench_verbose=0  # 默认，关闭热路径日志
 *   insmod day31_edu_bench.ko bench_verbose=1  # 开启，用于调试
 *
 * 为什么要关闭？
 *   bench-dma 每次运行触发 2 次 IRQ
 *   频繁打印会拖慢 QEMU -nographic 下的自动化
 */

#endif /* DAY31_EDU_BENCH_H */
