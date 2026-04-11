/*
 * day29_edu_dma.h - QEMU EDU DMA 驱动头文件
 *
 * ==================== 头文件作用 ====================
 *
 * 定义 Day29 DMA 驱动所需的常量、结构体、寄存器布局。
 *
 * 与 Day27 头文件相比，Day29 新增：
 *   - DMA 寄存器偏移（0x80~0x98）
 *   - DMA 命令位定义
 *   - DMA buffer 相关常量
 *   - day29_dev 中新增 DMA 相关字段
 *
 * ==================== 头文件内容 ====================
 *
 *  1. 模块和设备的命名常量
 *  2. EDU 寄存器布局（MMIO + DMA）
 *  3. DMA 相关常量
 *  4. day29_dev 结构体
 */

#ifndef DAY29_EDU_DMA_H
#define DAY29_EDU_DMA_H

/*
 * ==================== 头文件依赖 ====================
 *
 * linux/cdev.h    → struct cdev（字符设备）
 * linux/device.h → struct class, struct device（设备模型）
 * linux/mutex.h  → struct mutex（操作锁，保护 DMA 操作）
 * linux/pci.h     → struct pci_dev（PCI 设备）
 * linux/spinlock.h → struct spinlock（中断自旋锁）
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

/*
 * ==================== 第1部分：模块和设备命名常量 ====================
 *
 * 与 Day27 基本相同，只是名称反映 Day29 DMA 主题。
 */
#define DAY29_DRV_NAME              "day29_edu_dma"   /* 驱动名 */
#define DAY29_CLASS_NAME            "day29_edu"        /* sysfs 类名 */
#define DAY29_DEV_NAME_FMT          "day29_edu%d"      /* 设备节点名格式 */

/*
 * ==================== 第2部分：EDU 设备 PCI ID ====================
 *
 * 与 Day27 完全相同：Vendor=1234, Device=11e8
 */
#define DAY29_EDU_VENDOR_ID         0x1234
#define DAY29_EDU_DEVICE_ID         0x11e8

/*
 * ==================== 第3部分：EDU 寄存器布局 ====================
 *
 * 【两类寄存器】
 *   1. MMIO 寄存器（原有）：中断相关
 *   2. DMA 寄存器（新增）：DMA 传输控制
 *
 * 【MMIO 寄存器（与 Day27 相同）】
 *   0x00 ID：设备标识（只读）
 *   0x04 LIVENESS：存活验证（读写）
 *   0x24 IRQ_STATUS：中断状态（只读）
 *   0x64 IRQ_ACK：中断清除（只写）
 *
 * 【DMA 寄存器（Day29 新增）】
 *   0x80 DMA_SRC：DMA 源地址（写入，64-bit）
 *   0x88 DMA_DST：DMA 目的地址（写入，64-bit）
 *   0x90 DMA_COUNT：DMA 传输字节数（写入，32-bit）
 *   0x98 DMA_CMD：DMA 命令（写入，32-bit）
 */

/* MMIO 寄存器 */
#define DAY29_EDU_REG_IDENTITY      0x00   /* 设备ID（只读） */
#define DAY29_EDU_REG_LIVENESS      0x04   /* 存活验证（读写） */
#define DAY29_EDU_REG_IRQ_STATUS    0x24   /* 中断状态（只读） */
#define DAY29_EDU_REG_IRQ_ACK       0x64   /* 中断清除（只写） */

/* DMA 寄存器（Day29 新增）*/
#define DAY29_EDU_REG_DMA_SRC       0x80   /* DMA 源地址（写入，64-bit）*/
#define DAY29_EDU_REG_DMA_DST       0x88   /* DMA 目的地址（写入，64-bit）*/
#define DAY29_EDU_REG_DMA_COUNT     0x90   /* DMA 传输字节数（写入，32-bit）*/
#define DAY29_EDU_REG_DMA_CMD       0x98   /* DMA 命令（写入，32-bit）*/

/*
 * ==================== 第4部分：DMA 相关常量 ====================
 *
 * 【EDU 内部 buffer 偏移】
 *   EDU 设备内部有一个 buffer，偏移固定为 0x40000
 *   DMA 往返验证用这个地址作为中转
 *
 *   图示：
 *     RAM (src) → EDU(0x40000) → RAM (dst)
 */
#define DAY29_EDU_DEVBUF_OFFSET     0x40000ULL /* EDU 内部 buffer 偏移 */

/*
 * 【DMA 命令位定义】
 *
 *   bit0 (START)：启动 DMA 传输，写入 1 开始
 *   bit1 (DIR)：方向位，0=RAM→EDU, 1=EDU→RAM
 *   bit2 (IRQ)：DMA 完成后触发中断
 *
 * 【命令组合】
 *   第一次 DMA（RAM → EDU）：START | IRQ = 0x01 | 0x04 = 0x05
 *   第二次 DMA（EDU → RAM）：START | DIR | IRQ = 0x01 | 0x02 | 0x04 = 0x07
 */
#define DAY29_EDU_DMA_CMD_START     0x01   /* 启动 DMA */
#define DAY29_EDU_DMA_CMD_DIR_TO_RAM 0x02  /* 方向：EDU→RAM */
#define DAY29_EDU_DMA_CMD_IRQ       0x04   /* 完成触发中断 */

/*
 * 【DMA buffer 参数】
 *
 *   DMA mask：默认 32-bit（Day29 自动化放宽到 32）
 *   Buffer 大小：4096 bytes（4KB）
 *   Buffer 分区：前 2048 = src，后 2048 = dst
 *   最大验证长度：2048 bytes
 */
#define DAY29_DMA_MASK_BITS_DEFAULT 32     /* 默认 DMA mask 位数 */
#define DAY29_DMA_BYTES             4096   /* coherent buffer 大小（4KB）*/
#define DAY29_DMA_SRC_OFF           0       /* src 区偏移 */
#define DAY29_DMA_DST_OFF           2048    /* dst 区偏移 */
#define DAY29_DMA_VERIFY_MAX        2048    /* 最大验证长度 */

/*
 * ==================== 第5部分：day29_dev 结构体 ====================
 *
 * 【与 day27_dev 的区别】
 *
 * 新增 DMA 相关字段：
 *   - dma_virt/dma_handle/dma_bytes/dma_mask_bits
 *   - 验证结果字段（last_verify_*）
 *   - 操作锁（op_lock）
 *
 * 去掉字段：
 *   - 无（相比 day27 是在后面追加，不是去掉）
 */
struct day29_dev {
    /*
     * ─────────────── PCI 资源 ───────────────
     */
    struct pci_dev *pdev;          /* PCI 设备指针 */
    void __iomem *bar0;            /* BAR0 MMIO 映射虚拟地址 */
    resource_size_t bar0_start;     /* BAR0 起始物理地址 */
    resource_size_t bar0_len;      /* BAR0 长度 */

    /*
     * ─────────────── 中断资源 ───────────────
     */
    unsigned int irq_vector;       /* MSI 中断向量号 */
    u64 irq_count;                 /* 中断处理计数 */
    u32 last_irq_status;           /* 最近中断状态 */
    u32 last_ack_value;           /* 最近 ACK 值 */
    spinlock_t irq_lock;          /* 保护中断相关共享数据 */

    /*
     * ─────────────── DMA 资源（新增）──────────────
     *
     * 【dma_virt】
     *   CPU 访问用的虚拟地址
     *   用于 memset、fill_pattern、memcmp 等 CPU 操作
     *
     * 【dma_handle】
     *   设备访问用的 DMA 地址
     *   必须写入 DMA_SRC/DMA_DST 寄存器
     *   不能用于 CPU 内存访问（没有映射关系）
     *
     * 【dma_bytes】
     *   coherent buffer 大小，默认 4096
     *
     * 【dma_mask_bits】
     *   DMA mask 位数，默认 32
     */
    void *dma_virt;               /* CPU 访问用虚拟地址 */
    dma_addr_t dma_handle;        /* 设备访问用 DMA 地址 */
    size_t dma_bytes;             /* buffer 大小 */
    u32 dma_mask_bits;            /* DMA mask 位数 */

    /*
     * ─────────────── DMA 验证结果（新增）──────────────
     *
     * 这些字段在 day29_do_verify() 中被设置
     * 用于 ioctl(GET_VERIFY_RESULT) 返回给用户态
     */
    u32 last_verify_len;          /* 最近一次验证长度 */
    u32 last_verify_seed;         /* 最近一次验证 seed */
    s32 last_verify_error;        /* 验证错误码（0=成功）*/
    u32 last_verify_ok;           /* 验证是否成功（1=成功）*/
    s32 last_mismatch_index;      /* 第一个不匹配的偏移（-1=无不匹配）*/
    u8 last_mismatch_expected;    /* 期望值（用于调试）*/
    u8 last_mismatch_actual;      /* 实际值（用于调试）*/
    u32 last_irq_delta;           /* 验证期间 IRQ 增量（应为 2）*/
    u32 last_dma_cmd;             /* 最近一次 DMA 命令 */

    /*
     * ─────────────── 操作锁 ───────────────
     *
     * 保护 DMA 操作，防止并发导致数据错乱
     * DMA 往返验证必须完整执行，中间不能被打断
     */
    struct mutex op_lock;

    /*
     * ─────────────── 字符设备资源 ───────────────
     */
    dev_t devt;                   /* 设备号 */
    struct cdev cdev;            /* 字符设备结构 */
    struct device *device;        /* sysfs 设备节点 */
};

/*
 * 【Buffer 布局图解】
 *
 * 4KB coherent buffer
 * ┌────────────────────────┬─────────────────────────┐
 * │    src 区 [0~2047]      │    dst 区 [2048~4095]   │
 * │                         │                         │
 * │  用于存放源数据          │  用于接收 DMA 搬回的数据   │
 * │  fill_pattern() 填充    │  memset() 清零          │
 * │                         │                         │
 * │  DMA 源地址：            │  DMA 目的地址：           │
 * │  dma_handle + 0         │  dma_handle + 2048      │
 * └────────────────────────┴─────────────────────────┘
 *
 * 【为什么分两半？】
 *   - 同一块 buffer 内完成往返验证
 *   - 避免申请第二块 buffer
 *   - 验证时直接比较 src 和 dst
 */

#endif /* DAY29_EDU_DMA_H */
