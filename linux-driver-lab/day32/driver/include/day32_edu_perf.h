/* SPDX-License-Identifier: GPL-2.0 */
/*
 * day32_edu_perf.h - Day32 QEMU EDU perf/ftrace 驱动头文件
 *
 * ==================== 头文件作用 ====================
 *
 * 定义 Day32 驱动所需的常量、结构体、寄存器布局。
 *
 * 与 Day31 头文件相比，Day32 基本没有变化：
 *   - 模块参数名从 bench_verbose 改为 perf_verbose
 *   - 其他所有常量、结构体、寄存器布局与 Day31 完全相同
 *
 * Day32 的核心在用户态工具和 perf 脚本，不在驱动本身。
 *
 * ==================== 头文件内容 ====================
 *
 *  1. 模块和设备的命名常量
 *  2. EDU 寄存器布局（MMIO + DMA）
 *  3. DMA 相关常量
 *  4. day32_dev 结构体
 */

#ifndef DAY32_EDU_PERF_H
#define DAY32_EDU_PERF_H

/*
 * ==================== 头文件依赖 ====================
 *
 * 与 Day31 完全相同：
 *   linux/cdev.h    → struct cdev（字符设备）
 *   linux/device.h → struct class, struct device（设备模型）
 *   linux/mutex.h  → struct mutex（操作锁）
 *   linux/pci.h     → struct pci_dev（PCI 设备）
 *   linux/spinlock.h → struct spinlock（中断自旋锁）
 *
 * Day32 不需要额外的头文件：
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
 * 与 Day31 基本相同，只是名称反映 Day32 perf 主题。
 */
#define DAY32_DRV_NAME              "day32_edu_perf"  /* 驱动名 */
#define DAY32_CLASS_NAME            "day32_edu"        /* sysfs 类名 */
#define DAY32_DEV_NAME_FMT          "day32_edu%d"      /* 设备节点名格式 */

/*
 * ==================== 第2部分：EDU 设备 PCI ID ====================
 *
 * 与 Day31 完全相同：Vendor=1234, Device=11e8
 */
#define DAY32_EDU_VENDOR_ID         0x1234
#define DAY32_EDU_DEVICE_ID          0x11e8

/*
 * ==================== 第3部分：EDU 寄存器布局 ====================
 *
 * 与 Day31 完全相同：
 *   MMIO：0x00 ID, 0x04 LIVENESS, 0x24 IRQ_STATUS, 0x64 IRQ_ACK
 *   DMA：0x80 DMA_SRC, 0x88 DMA_DST, 0x90 DMA_COUNT, 0x98 DMA_CMD
 */

/* MMIO 寄存器 */
#define DAY32_EDU_REG_IDENTITY      0x00
#define DAY32_EDU_REG_LIVENESS      0x04
#define DAY32_EDU_REG_IRQ_STATUS    0x24
#define DAY32_EDU_REG_IRQ_ACK       0x64

/* DMA 寄存器 */
#define DAY32_EDU_REG_DMA_SRC       0x80
#define DAY32_EDU_REG_DMA_DST       0x88
#define DAY32_EDU_REG_DMA_COUNT     0x90
#define DAY32_EDU_REG_DMA_CMD       0x98

/*
 * ==================== 第4部分：DMA 相关常量 ====================
 *
 * 与 Day31 完全相同：
 *   DEVBUF_OFFSET = 0x40000
 *   DMA 命令位：START=0x01, DIR_TO_RAM=0x02, IRQ=0x04
 *   Buffer：4096 bytes，src 偏移 0，dst 偏移 2048
 */
#define DAY32_EDU_DEVBUF_OFFSET        0x40000ULL
#define DAY32_EDU_DMA_CMD_START        0x01
#define DAY32_EDU_DMA_CMD_DIR_TO_RAM   0x02
#define DAY32_EDU_DMA_CMD_IRQ          0x04

#define DAY32_DMA_MASK_BITS_DEFAULT    32
#define DAY32_DMA_BYTES                4096
#define DAY32_DMA_SRC_OFF              0
#define DAY32_DMA_DST_OFF              2048
#define DAY32_DMA_VERIFY_MAX           2048

/*
 * ==================== 第5部分：day32_dev 结构体 ====================
 *
 * 【与 day31_dev 完全相同】
 *
 * Day32 的驱动结构体与 Day31 完全相同，
 * 区别仅在于名称（day31 → day32）。
 *
 * 新增 bench 统计字段（与 Day31 相同）：
 *   - total_run_calls：RUN_DMA 累计调用次数
 *   - total_run_ok：累计成功次数
 *   - total_run_fail：累计失败次数
 *   - last_run_ns：最近一次 DMA 运行的纳秒级耗时
 *
 * 这些字段用于：
 *   - 用户态 bench 工具查询累计成功率
 *   - 内核记录精确的 DMA 耗时
 *   - 配合用户态计时分析性能瓶颈
 *
 * 【为什么驱动结构体没变？】
 *   Day32 的优化点在用户态 bench 工具，
 *   不在驱动。驱动继续提供 Day31 的所有功能。
 */
struct day32_dev {
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
     * ─────────────── Bench 统计 ───────────────
     *
     * 【与 Day31 完全相同的字段】
     *   total_run_calls：RUN_DMA 累计调用次数
     *   total_run_ok：累计成功次数
     *   total_run_fail：累计失败次数
     *   last_run_ns：最近一次 DMA 耗时（纳秒）
     *
     * 这些字段在 Day32 依然有用：
     *   - 用户态 bench 工具通过 ioctl GET_INFO 获取
     *   - 用于验证 DMA 往返的正确性和稳定性
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
 * 【Day32 的核心主题：mmap 复用优化】
 *
 * Day32 不在驱动做优化，而是在用户态 bench 工具做对比：
 *
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │                    Day32 mmap 复用优化                                │
 * ├─────────────────────────────────────────────────────────────────────┤
 * │                                                                      │
 * │  baseline（每次迭代都 mmap/munmap）：                                │
 * │  ────────────────────────────────────                               │
 * │  for iter:                                                          │
 * │      get_info()           ← syscall                                 │
 * │      mmap()               ← syscall + VMA 创建                      │
 * │      memcpy()             ← 用户态内存操作                           │
 * │      memcmp()             ← 用户态内存操作                          │
 * │      munmap()             ← syscall + VMA 销毁                     │
 * │                                                                      │
 * │  optimized（提前 mmap，循环内不复用）：                               │
 * │  ────────────────────────────────────                               │
 * │  get_info()               ← 只做一次                               │
 * │  mmap()                   ← 只做一次                                 │
 * │  for iter:                                                           │
 * │      memcpy()             ← 只有这个在热路径                        │
 * │      memcmp()             ← 只有这个在热路径                        │
 * │  munmap()                 ← 只做一次                                 │
 * │                                                                      │
 * └─────────────────────────────────────────────────────────────────────┘
 *
 * 【mmap+munmap 成本分析】
 *
 *   每次 mmap+munmap 循环涉及：
 *     - 2 个 syscall（用户态/内核态切换）
 *     - 2 个 VMA 创建/销毁（内核红黑树操作）
 *     - 页表更新（触发 page fault）
 *     - 内核锁竞争
 *
 *   当 len=256 字节时：
 *     - memcpy 256 字节 ≈ 0.05 微秒
 *     - mmap + munmap ≈ 280 微秒
 *
 *   所以搬出 mmap 能带来 99%+ 的性能提升。
 */

/*
 * 【Day32 perf 工具链】
 *
 * Day32 使用宿主端 perf 工具验证优化效果：
 *
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │                      perf 工具链用法                                 │
 * ├─────────────────────────────────────────────────────────────────────┤
 * │                                                                      │
 * │  perf stat -d -o output.stat.txt ./bench-mmap                       │
 * │  ────────────────────────────────────────────                       │
 * │  统计计数器：cycles, instructions, cache-misses, branch-mispredict  │
 * │                                                                      │
 * │  perf record -F 49 -g -o output.data ./bench-mmap                   │
 * │  ────────────────────────────────────────────                       │
 * │  -F 49：每秒 49 次采样（避免干扰）                                   │
 * │  -g：记录调用链                                                     │
 * │                                                                      │
 * │  perf report --stdio -i output.data                                 │
 * │  ───────────────────────────────────                               │
 * │  显示热点函数，按 CPU 占用排序                                        │
 * │                                                                      │
 * └─────────────────────────────────────────────────────────────────────┘
 *
 * 【baseline vs optimized 的 perf 预期差异】
 *
 *   baseline perf 报告：
 *     - mmap/munmap syscall 占比高
 *     - VMA 相关函数占比高（mlock_lock, vm_area_struct 操作）
 *
 *   optimized perf 报告：
 *     - memcpy/memcmp 占比高
 *     - mmap/munmap 热点消失（不在热路径）
 */

/*
 * 【模块参数：perf_verbose】
 *
 * Day32 新增 perf_verbose 模块参数：
 *
 *   insmod day32_edu_perf.ko                # 默认，关闭热路径日志
 *   insmod day32_edu_perf.ko perf_verbose=1 # 开启，用于调试
 *
 * 为什么要关闭？
 *   baseline 每次迭代触发 2 次 IRQ（两段 DMA）
 *   频繁打印会拖慢 QEMU -nographic 下的自动化
 *
 * 为什么叫 perf_verbose？
 *   Day32 主题是 perf/ftrace，
 *   这个名称反映主题（语义等同于 Day31 的 bench_verbose）。
 */

#endif /* DAY32_EDU_PERF_H */
