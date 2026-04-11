/* SPDX-License-Identifier: GPL-2.0 */
/*
 * day30_edu_mmap.h - Day30 QEMU EDU mmap 零拷贝驱动头文件
 *
 * ==================== 头文件作用 ====================
 *
 * 定义 Day30 驱动所需的常量、结构体、寄存器布局。
 *
 * 与 Day29 头文件相比，Day30 新增：
 *   - mmap 边界校验相关常量
 *   - last_mmap_* 字段（mmap 结果追踪）
 *   - 整体结构与 Day29 相似，但注释反映 mmap 主题
 *
 * ==================== 头文件内容 ====================
 *
 *  1. 模块和设备的命名常量
 *  2. EDU 寄存器布局（MMIO + DMA）
 *  3. DMA 相关常量
 *  4. day30_dev 结构体
 */

#ifndef DAY30_EDU_MMAP_H
#define DAY30_EDU_MMAP_H

/*
 * ==================== 头文件依赖 ====================
 *
 * linux/cdev.h    → struct cdev（字符设备）
 * linux/device.h → struct class, struct device（设备模型）
 * linux/mutex.h  → struct mutex（操作锁，保护 DMA 操作）
 * linux/pci.h     → struct pci_dev（PCI 设备）
 * linux/spinlock.h → struct spinlock（中断自旋锁）
 *
 * 注意：Day30 不需要额外的 DMA 相关头文件
 * 因为 dma_alloc_coherent、dma_mmap_coherent 等已经在
 * linux/dma-mapping.h 中声明（在 .c 文件中 include）
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

/*
 * ==================== 第1部分：模块和设备命名常量 ====================
 *
 * 与 Day29 基本相同，只是名称反映 Day30 mmap 主题。
 */
#define DAY30_DRV_NAME              "day30_edu_mmap"  /* 驱动名 */
#define DAY30_CLASS_NAME            "day30_edu"        /* sysfs 类名 */
#define DAY30_DEV_NAME_FMT          "day30_edu%d"      /* 设备节点名格式 */

/*
 * ==================== 第2部分：EDU 设备 PCI ID ====================
 *
 * 与 Day29 完全相同：Vendor=1234, Device=11e8
 */
#define DAY30_EDU_VENDOR_ID         0x1234
#define DAY30_EDU_DEVICE_ID         0x11e8

/*
 * ==================== 第3部分：EDU 寄存器布局 ====================
 *
 * 【MMIO 寄存器（与 Day29 相同）】
 *   0x00 ID：设备标识（只读）
 *   0x04 LIVENESS：存活验证（读写）
 *   0x24 IRQ_STATUS：中断状态（只读）
 *   0x64 IRQ_ACK：中断清除（只写）
 *
 * 【DMA 寄存器（与 Day29 相同）】
 *   0x80 DMA_SRC：DMA 源地址（写入，64-bit）
 *   0x88 DMA_DST：DMA 目的地址（写入，64-bit）
 *   0x90 DMA_COUNT：DMA 传输字节数（写入，32-bit）
 *   0x98 DMA_CMD：DMA 命令（写入，32-bit）
 */

/* MMIO 寄存器 */
#define DAY30_EDU_REG_IDENTITY      0x00   /* 设备ID（只读） */
#define DAY30_EDU_REG_LIVENESS      0x04   /* 存活验证（读写） */
#define DAY30_EDU_REG_IRQ_STATUS    0x24   /* 中断状态（只读） */
#define DAY30_EDU_REG_IRQ_ACK       0x64   /* 中断清除（只写） */

/* DMA 寄存器 */
#define DAY30_EDU_REG_DMA_SRC       0x80   /* DMA 源地址（写入，64-bit）*/
#define DAY30_EDU_REG_DMA_DST       0x88   /* DMA 目的地址（写入，64-bit）*/
#define DAY30_EDU_REG_DMA_COUNT     0x90   /* DMA 传输字节数（写入，32-bit）*/
#define DAY30_EDU_REG_DMA_CMD       0x98   /* DMA 命令（写入，32-bit）*/

/*
 * ==================== 第4部分：DMA 与 mmap 相关常量 ====================
 *
 * 【EDU 内部 buffer 偏移】
 *   EDU 设备内部有一个 buffer，偏移固定为 0x40000
 *   DMA 往返验证用这个地址作为中转
 *
 * 【DMA 命令位定义】
 *   bit0 (START)：启动 DMA 传输，写入 1 开始
 *   bit1 (DIR)：方向位，0=RAM→EDU, 1=EDU→RAM
 *   bit2 (IRQ)：DMA 完成后触发中断
 *
 * 【DMA buffer 参数】
 *   Buffer 大小：4096 bytes（4KB）
 *   Buffer 分区：前 2048 = src，后 2048 = dst
 *   最大验证长度：2048 bytes
 *   默认 DMA mask：32-bit（Day30 自动化放宽）
 */

/* EDU 内部 buffer 偏移 */
#define DAY30_EDU_DEVBUF_OFFSET        0x40000ULL

/* DMA 命令位定义 */
#define DAY30_EDU_DMA_CMD_START        0x01   /* 启动 DMA */
#define DAY30_EDU_DMA_CMD_DIR_TO_RAM   0x02   /* 方向：EDU→RAM */
#define DAY30_EDU_DMA_CMD_IRQ          0x04   /* 完成触发中断 */

/* DMA buffer 参数 */
#define DAY30_DMA_MASK_BITS_DEFAULT    32     /* 默认 DMA mask 位数 */
#define DAY30_DMA_BYTES                4096   /* coherent buffer 大小（4KB）*/
#define DAY30_DMA_SRC_OFF             0       /* src 区偏移 */
#define DAY30_DMA_DST_OFF             2048    /* dst 区偏移 */
#define DAY30_DMA_VERIFY_MAX          2048    /* 最大验证长度 */

/*
 * ==================== 第5部分：day30_dev 结构体 ====================
 *
 * 【与 day29_dev 的区别】
 *
 * 新增 mmap 结果字段：
 *   - last_mmap_ok：mmap 是否成功
 *   - last_mmap_error：mmap 错误码
 *   - last_mmap_len：mmap 请求的长度
 *   - last_mmap_pgoff：mmap 请求的页偏移
 *
 * 去掉字段：
 *   - 无（相比 day29 是在后面追加，不是去掉）
 *
 * 【Day30 vs Day29 的职责变化】
 *   Day29：内核 fill_pattern、memset、compare（内核是主角）
 *   Day30：用户态通过 mmap 直接 fill/clear/compare（用户态是主角）
 */
struct day30_dev {
    /*
     * ─────────────── PCI 资源 ───────────────
     */
    struct pci_dev *pdev;          /* PCI 设备指针 */
    void __iomem *bar0;            /* BAR0 MMIO 映射虚拟地址 */
    resource_size_t bar0_start;    /* BAR0 起始物理地址 */
    resource_size_t bar0_len;      /* BAR0 长度 */

    /*
     * ─────────────── 中断资源 ───────────────
     *
     * 与 Day29 完全相同
     */
    unsigned int irq_vector;       /* MSI 中断向量号 */
    u64 irq_count;                 /* 中断处理计数 */
    u32 last_irq_status;           /* 最近中断状态 */
    u32 last_ack_value;            /* 最近 ACK 值 */
    spinlock_t irq_lock;           /* 保护中断相关共享数据 */

    /*
     * ─────────────── DMA 资源 ───────────────
     *
     * 与 Day29 完全相同
     * 这些资源会被 mmap 暴露给用户态
     */
    void *dma_virt;                /* CPU 访问用虚拟地址 */
    dma_addr_t dma_handle;         /* 设备访问用 DMA 地址 */
    size_t dma_bytes;              /* buffer 大小 */
    u32 dma_mask_bits;             /* DMA mask 位数 */

    /*
     * ─────────────── DMA 运行结果 ───────────────
     *
     * 与 Day29 类似，但含义略有不同：
     *   Day29：last_verify_* 表示"内核验证"结果
     *   Day30：last_run_* 表示"内核发起 DMA"的结果
     *
     * 注意：Day30 的 compare 在用户态做，不是内核
     */
    u32 last_run_len;              /* 最近一次运行长度 */
    u32 last_run_seed;             /* 最近一次运行 seed */
    s32 last_run_error;            /* 运行错误码（0=成功）*/
    u32 last_run_ok;               /* 运行是否成功（1=成功）*/
    u32 last_irq_delta;            /* 运行期间 IRQ 增量（应为 2）*/
    u32 last_dma_cmd;              /* 最近一次 DMA 命令 */

    /*
     * ─────────────── mmap 结果（新增）──────────────
     *
     * 【为什么需要 mmap 结果字段？】
     *   mmap 可能因为多种原因失败：
     *     - pgoff != 0         → -EINVAL
     *     - len != map_bytes    → -EINVAL
     *     - dma_mmap_coherent 内部错误
     *   这些信息需要记录并返回给用户态
     *
     * 【mmap 边界校验规则】
     *   Day30 只允许：
     *     - offset == 0
     *     - length == PAGE_ALIGN(dma_bytes) == 4096
     */
    u32 last_mmap_ok;              /* mmap 是否成功（1=成功）*/
    s32 last_mmap_error;           /* mmap 错误码 */
    u32 last_mmap_len;             /* mmap 请求的长度 */
    u32 last_mmap_pgoff;           /* mmap 请求的页偏移 */

    /*
     * ─────────────── 操作锁 ───────────────
     *
     * 保护 DMA 操作，防止并发导致数据错乱
     * 注意：mmap 本身不需要锁（每个进程有独立的 VMA）
     */
    struct mutex op_lock;

    /*
     * ─────────────── 字符设备资源 ───────────────
     */
    dev_t devt;                    /* 设备号 */
    struct cdev cdev;              /* 字符设备结构 */
    struct device *device;           /* sysfs 设备节点 */
};

/*
 * 【Day30 Buffer 布局图解】
 *
 * 与 Day29 相同：
 * 4KB coherent buffer
 * ┌────────────────────────┬─────────────────────────┐
 * │    src 区 [0~2047]      │    dst 区 [2048~4095]   │
 * │                         │                         │
 * │  用于存放源数据          │  用于接收 DMA 搬回的数据   │
 * │                         │                         │
 * │  fill_pattern() 填充    │  memset() 清零          │
 * │  （用户态 mmap 后直接写）  │  （用户态 mmap 后直接清）  │
 * │                         │                         │
 * └────────────────────────┴─────────────────────────┘
 *
 * 【Day30 vs Day29 的核心区别】
 *
 *   Day29：
 *     内核 fill_pattern() → 内核 memset() → 内核 compare()
 *     内核是主角
 *
 *   Day30：
 *     用户态 mmap() 后直接 fill_pattern() / memset() / compare()
 *     用户态是主角
 *     内核只负责 mmap 映射 + DMA 编程
 *
 * 【mmap 页对齐陷阱】
 *
 *   mmap() 的 length 参数在建立 VMA 时会按页向上取整
 *
 *   4KB 页环境下：
 *     请求 2048 字节 → 实际分配 4096 字节
 *     请求 4096 字节 → 实际分配 4096 字节
 *     请求 4097 字节 → 实际分配 8192 字节
 *
 *   因此 invalid-length 测试要用 4097 这类"跨页但又不等于 map_bytes"的长度
 */

#endif /* DAY30_EDU_MMAP_H */
