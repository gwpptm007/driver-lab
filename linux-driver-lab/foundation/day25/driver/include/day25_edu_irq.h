/*
 * day25_edu_irq.h - EDU MSI 中断实验驱动头文件
 *
 * ==================== 头文件作用 ====================
 *
 * 头文件定义了三类内容：
 *   1. 模块和设备的命名常量（驱动名、class 名、设备节点名格式）
 *   2. EDU 寄存器布局常量（EDU 硬件的寄存器地址映射）
 *   3. day25_dev 结构体（驱动私有的运行时数据结构）
 *
 * 头文件 vs .c 文件的分工：
 *   .h：定义常量、结构体、寄存器布局（可被其他文件 include）
 *   .c：实现驱动逻辑（probe/remove/ioctl handler 等）
 */

#ifndef DAY25_EDU_IRQ_H
#define DAY25_EDU_IRQ_H

/*
 * ==================== 头文件依赖 ====================
 *
 * linux/cdev.h    → struct cdev（字符设备结构体）
 * linux/pci.h     → struct pci_dev、pci_iomap 等 PCI API
 * linux/spinlock.h → struct spinlock（自旋锁）
 *
 * 注意：用户态程序用 day25_edu_uapi.h，不是这个头文件
 *       day25_edu_uapi.h 定义的是用户态和驱动态共享的 ioctl 编号和数据结构
 */
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

/*
 * ==================== 第1部分：模块和设备命名常量 ====================
 *
 * 这些常量定义了驱动在系统中的"名字"
 * 会在以下地方出现：
 *   /sys/class/day25_edu/        ← DAY25_CLASS_NAME
 *   /sys/bus/pci/drivers/day25_edu_irq/  ← DAY25_DRV_NAME
 *   /dev/day25_edu0              ← DAY25_DEV_NAME_FMT
 *   /proc/interrupts             ← request_irq 的 name 参数
 */
#define DAY25_DRV_NAME     "day25_edu_irq"    /* PCI 驱动名（在 /sys/bus/pci/drivers/ 下） */
#define DAY25_CLASS_NAME   "day25_edu"        /* sysfs class 名（在 /sys/class/ 下） */
#define DAY25_DEV_NAME_FMT "day25_edu%d"      /* 设备节点名格式（%d = 次设备号） */
#define DAY25_MAX_MINORS   32                  /* 最多支持的次设备号数量（可扩展多设备） */

/*
 * ==================== 第2部分：EDU 设备 PCI ID ====================
 *
 * QEMU 模拟的 EDU 教学设备：
 *   Vendor ID:  0x1234（QEMU 的虚拟厂商 ID）
 *   Device ID:  0x11e8（EDU 教学设备 ID）
 *
 * 内核通过这两个 ID 匹配设备：
 *   PCI 总线枚举时，发现 1234:11e8 设备
 *   → 在 pci_device_id 数组中找匹配项
 *   → 找到后调用对应驱动的 probe()
 */
#define DAY25_EDU_VENDOR_ID  0x1234
#define DAY25_EDU_DEVICE_ID  0x11e8

/*
 * ==================== 第3部分：EDU BAR0 寄存器布局 ====================
 *
 * EDU 设备的控制寄存器都在 BAR0（4KB 空间）
 * 我们只用到其中几个寄存器：
 *
 * EDU 寄存器都是 32-bit MMIO 寄存器
 * 访问方式：readl(BAR0虚拟地址 + 偏移) / writel(值, BAR0虚拟地址 + 偏移)
 *
 * 寄存器分类：
 *   【只读】ID         → 设备ID，验证驱动和硬件连接正常
 *   【读写】LIVENESS   → 写入测试值，验证 MMIO 映射正确
 *   【只读】STATUS/IRQ_STATUS → 设备状态和中断状态
 *   【只写】IRQ_RAISE  → 触发 MSI 中断的核心寄存器
 *   【只写】IRQ_ACK    → 清除中断的寄存器
 */

/* BAR0 编号：PCI 有 6 个 BAR (0~5)，EDU 只用 BAR0 */
#define DAY25_BAR0 0

/*
 * EDU MMIO 寄存器布局（仅列出 day25 用到的）：
 *
 * 偏移      名称           读写属性    说明
 * ──────────────────────────────────────────────────────────
 * 0x00     ID             只读       设备ID，值固定为 EDU 设备标识
 * 0x04     LIVENESS       读写       写入 0xa5a55a5a，读回值为按位取反
 * 0x20     STATUS         只读       设备状态寄存器
 * 0x24     IRQ_STATUS     只读       当前等待中的中断源（位掩码）
 * 0x60     IRQ_RAISE      只写       写入任意值触发 MSI 中断
 * 0x64     IRQ_ACK       只写       写入 IRQ_STATUS 的值清除中断
 */
#define DAY25_EDU_REG_ID          0x00   /* 设备ID（只读） */
#define DAY25_EDU_REG_LIVENESS    0x04   /* 存活验证（写入测试值，读回应为取反）*/
#define DAY25_EDU_REG_STATUS      0x20   /* 设备状态（只读） */
#define DAY25_EDU_REG_IRQ_STATUS  0x24   /* 中断状态（只读，位掩码） */
#define DAY25_EDU_REG_IRQ_RAISE   0x60   /* 触发中断（只写） ← day25 核心！ */
#define DAY25_EDU_REG_IRQ_ACK     0x64   /* 清除中断（只写） */

/*
 * ==================== 第4部分：day25_dev 结构体 ====================
 *
 * 每个被驱动的 EDU 设备都对应一个 day25_dev 实例
 * probe() 时分配，remove() 时释放
 *
 * 结构体包含三类成员：
 *   1. PCI 资源：pdev、bar0、bar0_start、bar0_len
 *   2. 中断资源：irq_vector、irq_count、last_irq_status、last_ack_value、irq_lock
 *   3. 字符设备资源：devt、cdev、device
 *
 * 为什么把所有东西放一个结构体？
 *   → 方便 probe/remove 完整管理所有资源
 *   → file->private_data 只需存一个指针
 */
struct day25_dev {
    /*
     * ─────────────── PCI 资源 ───────────────
     */
    struct pci_dev *pdev;          /* 指向 PCI 设备结构体的指针 */

    void __iomem *bar0;            /* BAR0 MMIO 映射后的虚拟地址 */
                                   /* 访问方式：readl(bar0 + 偏移) */
    resource_size_t bar0_start;     /* BAR0 起始物理地址（来自 pci_resource_start）*/
    resource_size_t bar0_len;      /* BAR0 长度（来自 pci_resource_len）*/

    /*
     * ─────────────── 中断资源 ───────────────
     */
    int irq_vector;                /* Linux 内核分配的中断向量号（IRQ 号） */
                                   /* 用于 request_irq() 和 free_irq() */
    u64 irq_count;                 /* 中断处理函数被调用的次数（handler 中自增）*/

    u32 last_irq_status;           /* 最近一次中断的 IRQ_STATUS 值（handler 中记录）*/
    u32 last_ack_value;            /* 最近一次清除中断时写入 IRQ_ACK 的值 */

    u32 liveness_value;            /* 写入 LIVENESS 的测试值（0xa5a55a5a）*/
    u32 liveness_inverted;         /* 读回的按位取反值（应为 0x5a5aa5a5）*/

    spinlock_t irq_lock;           /* 保护 irq_count/status/ack 的自旋锁 */
                                   /* handler 和 ioctl 都会访问这些共享变量 */

    /*
     * ─────────────── 字符设备资源 ───────────────
     */
    dev_t devt;                    /* 设备号（主设备号+次设备号）*/
                                   /* 由 MKDEV(MAJOR, minor) 生成 */
    struct cdev cdev;              /* 字符设备结构体（VFS 层使用）*/
                                   /* cdev_init() 绑定 file_operations */
    struct device *device;          /* 指向 device_create() 创建的设备 */
                                   /* 用于 sysfs 和 udev 创建设备节点 */
};

/*
 * 为什么没有包含 ioctl 编号定义？
 *   ioctl 编号定义在 day25_edu_uapi.h 中
 *   因为 ioctl 编号需要被用户态程序 include（用户态不能 include .h）
 *   所以 uapi 头文件被设计成"用户态和驱动共享"
 */

#endif /* DAY25_EDU_IRQ_H */
