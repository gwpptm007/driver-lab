/* SPDX-License-Identifier: GPL-2.0 */
/*
 * day33_edu_trace.h - Day33 QEMU EDU ftrace function_graph 驱动头文件
 *
 * ==================== 头文件作用 ====================
 *
 * 定义 Day33 驱动所需的常量、结构体、寄存器布局。
 *
 * 与 Day32 头文件相比，Day33 基本没有变化：
 *   - 模块参数名从 perf_verbose 改为 trace_verbose
 *   - 其他所有常量、结构体、寄存器布局与 Day32 完全相同
 *
 * Day33 的核心在 ftrace 配置和 trace 阅读，不在驱动本身。
 *
 * ==================== 头文件内容 ====================
 *
 *  1. 模块和设备的命名常量
 *  2. EDU 寄存器布局（MMIO + DMA）
 *  3. DMA 相关常量
 *  4. day33_dev 结构体
 */

#ifndef DAY33_EDU_TRACE_H
#define DAY33_EDU_TRACE_H

/*
 * ==================== 头文件依赖 ====================
 *
 * 与 Day32 完全相同：
 *   linux/cdev.h    → struct cdev（字符设备）
 *   linux/device.h → struct class, struct device（设备模型）
 *   linux/mutex.h  → struct mutex（操作锁）
 *   linux/pci.h     → struct pci_dev（PCI 设备）
 *   linux/spinlock.h → struct spinlock（中断自旋锁）
 *
 * Day33 不需要额外的头文件。
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

/*
 * ==================== 第1部分：模块和设备命名常量 ====================
 *
 * 与 Day32 基本相同，只是名称反映 Day33 ftrace 主题。
 */
#define DAY33_DRV_NAME              "day33_edu_trace"  /* 驱动名 */
#define DAY33_CLASS_NAME            "day33_edu"        /* sysfs 类名 */
#define DAY33_DEV_NAME_FMT          "day33_edu%d"      /* 设备节点名格式 */

/*
 * ==================== 第2部分：EDU 设备 PCI ID ====================
 *
 * 与 Day32 完全相同：Vendor=1234, Device=11e8
 */
#define DAY33_EDU_VENDOR_ID         0x1234
#define DAY33_EDU_DEVICE_ID          0x11e8

/*
 * ==================== 第3部分：EDU 寄存器布局 ====================
 *
 * 与 Day32 完全相同：
 *   MMIO：0x00 ID, 0x04 LIVENESS, 0x24 IRQ_STATUS, 0x64 IRQ_ACK
 *   DMA：0x80 DMA_SRC, 0x88 DMA_DST, 0x90 DMA_COUNT, 0x98 DMA_CMD
 */

/* MMIO 寄存器 */
#define DAY33_EDU_REG_IDENTITY      0x00
#define DAY33_EDU_REG_LIVENESS      0x04
#define DAY33_EDU_REG_IRQ_STATUS    0x24
#define DAY33_EDU_REG_IRQ_ACK       0x64

/* DMA 寄存器 */
#define DAY33_EDU_REG_DMA_SRC       0x80
#define DAY33_EDU_REG_DMA_DST       0x88
#define DAY33_EDU_REG_DMA_COUNT     0x90
#define DAY33_EDU_REG_DMA_CMD       0x98

/*
 * ==================== 第4部分：DMA 相关常量 ====================
 *
 * 与 Day32 完全相同：
 *   DEVBUF_OFFSET = 0x40000
 *   DMA 命令位：START=0x01, DIR_TO_RAM=0x02, IRQ=0x04
 *   Buffer：4096 bytes，src 偏移 0，dst 偏移 2048
 */
#define DAY33_EDU_DEVBUF_OFFSET        0x40000ULL
#define DAY33_EDU_DMA_CMD_START        0x01
#define DAY33_EDU_DMA_CMD_DIR_TO_RAM   0x02
#define DAY33_EDU_DMA_CMD_IRQ          0x04

#define DAY33_DMA_MASK_BITS_DEFAULT    32
#define DAY33_DMA_BYTES                4096
#define DAY33_DMA_SRC_OFF              0
#define DAY33_DMA_DST_OFF              2048
#define DAY33_DMA_VERIFY_MAX           2048

/*
 * ==================== 第5部分：day33_dev 结构体 ====================
 *
 * 【与 day32_dev 完全相同】
 *
 * Day33 的驱动结构体与 Day32 完全相同，
 * 区别仅在于名称（day32 → day33）。
 *
 * Day33 不在驱动做改动，而是通过 ftrace 解释已跑通的路径。
 */
struct day33_dev {
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
     * 【与 Day32 完全相同的字段】
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
 * 【Day33 的核心主题：ftrace function_graph 调用路径解释】
 *
 * Day33 不做新的功能开发，而是用 ftrace 看清已跑通路径的调用关系：
 *
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │                  Day33 ftrace function_graph 路径                     │
 * ├─────────────────────────────────────────────────────────────────────┤
 * │                                                                      │
 * │  用户态:                                                             │
 * │      ioctl(fd, RUN_DMA, &req)                                        │
 * │          ↓                                                            │
 * │      kernel: sys_ioctl()                                             │
 * │          ↓                                                            │
 * │      day33_ioctl()            ←── function_graph 入口                 │
 * │          ↓                                                            │
 * │      day33_do_run_dma()       ←── 主逻辑（两段 DMA）                   │
 * │          ↓                                                            │
 * │      ├→ day33_program_dma()  stage 1: RAM → EDU                     │
 * │      │       ↓                                                      │
 * │      │   day33_wait_dma_idle()  ←── 轮询等待（耗时最长）              │
 * │      │       ↓                                                      │
 * │      │   [IRQ 触发]                                                 │
 * │      │       ↓                                                      │
 * │      │   day33_irq_handler()   ←── 中断处理（很快）                   │
 * │      │                                                             │
 * │      └→ day33_program_dma()  stage 2: EDU → RAM                     │
 * │              ↓                                                      │
 * │          day33_wait_dma_idle()                                      │
 * │              ↓                                                      │
 * │          [IRQ 触发]                                                 │
 * │              ↓                                                      │
 * │          day33_irq_handler()                                        │
 * │          ↓                                                            │
 * │      return 0                                                        │
 * │          ↓                                                            │
 * │      用户态 memcmp(src, dst, len)  ←── 验证结果                       │
 * │                                                                      │
 * └─────────────────────────────────────────────────────────────────────┘
 *
 * 【关键观察点】
 *
 *   1. day33_ioctl 自身执行时间很短（只做命令分发）
 *   2. day33_do_run_dma 的时间 ≈ 两段 DMA 的总时间
 *   3. day33_wait_dma_idle 是耗时最长的函数（轮询设备）
 *   4. day33_irq_handler 执行时间很短（只读状态+ACK）
 */

/*
 * 【ftrace function_graph 输出格式】
 *
 *   # tracer: function_graph
 *   #
 *   # CPU  DURATION                  FUNCTION CALLS
 *   # |     |                           |
 *     0)               |      day33_ioctl() {
 *     0)               |        day33_do_run_dma() {
 *     0)   0.521 us    |          day33_program_dma() {
 *     0)   0.123 us    |            day33_wait_dma_idle();
 *     0)   0.832 us    |          }
 *     0)               |          day33_irq_handler() {
 *     0)   0.234 us    |          }
 *     0)               |          day33_program_dma() {
 *     0)   0.456 us    |            day33_wait_dma_idle();
 *     0)   1.234 us    |          }
 *     0)               |          day33_irq_handler() {
 *     0)   0.198 us    |          }
 *     0) + 52.123 us   |        }
 *     0)               |      }
 *
 * 【Duration 列解读】
 *
 *   "0.521 us"：函数自身执行时间（不含子函数）
 *   "+ 52.123 us"：函数总执行时间（含子函数，"+"表示打印时函数还在运行）
 */

/*
 * 【模块参数：trace_verbose】
 *
 * Day33 新增 trace_verbose 模块参数：
 *
 *   insmod day33_edu_trace.ko              # 默认，关闭热路径日志
 *   insmod day33_edu_trace.ko trace_verbose=1  # 开启，用于调试
 *
 * 为什么要关闭？
 *   如果开启，每次 IRQ 都会 dev_info() 打印，
 *   这会拖慢 trace 采集和 QEMU -nographic 自动化。
 *
 * 为什么叫 trace_verbose？
 *   Day33 主题是 ftrace tracer，
 *   这个名称反映主题（语义等同于 Day31 的 bench_verbose、
 *   Day32 的 perf_verbose）。
 */

/*
 * 【Day33 与 Day32 的区别】
 *
 *   Day32：用 perf 测量"哪个函数最热"
 *   Day33：用 ftrace 看清"函数是怎么调用的"
 *
 *   Day32 做了 mmap 复用优化
 *   Day33 不做新优化，只是解释已跑通的路径
 */

#endif /* DAY33_EDU_TRACE_H */
