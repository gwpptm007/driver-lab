/*
 * day27_edu_loop.h - EDU 循环卸载稳定性测试驱动头文件
 *
 * ==================== 头文件作用 ====================
 *
 * 定义 Day27 驱动所需的常量、结构体、寄存器布局。
 *
 * 与 Day26 头文件相比，Day27 更精简：
 *   - 去掉 identity_value、liveness_value、liveness_inverted 字段
 *   - 只保留中断相关核心字段
 *   原因：Day27 追求最小化，200 次循环不需要那些额外字段
 *
 * ==================== 头文件内容 ====================
 *
 *  1. 模块和设备的命名常量（驱动名、class 名、设备节点名格式）
 *  2. EDU 寄存器布局常量（EDU 硬件的寄存器地址映射）
 *  3. day27_dev 结构体（驱动私有的运行时数据结构）
 */

#ifndef DAY27_EDU_LOOP_H
#define DAY27_EDU_LOOP_H

/*
 * ==================== 头文件依赖 ====================
 *
 * linux/cdev.h    → struct cdev（字符设备结构体）
 * linux/device.h → struct class, struct device（设备模型）
 * linux/pci.h     → struct pci_dev、pci_iomap 等 PCI API
 * linux/spinlock.h → struct spinlock（自旋锁）
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

/*
 * ==================== 第1部分：模块和设备命名常量 ====================
 *
 * 这些常量定义了驱动在系统中的"名字"
 *
 * 【与 Day26 的区别】
 *   Day26: DAY26_DRV_NAME="day26_edu_tool", DAY26_CLASS_NAME="day26_edu"
 *   Day27: DAY27_DRV_NAME="day27_edu_loop", DAY27_CLASS_NAME="day27_edu"
 *
 * 【为什么命名不同？】
 *   → 每个实验日是独立的，命名反映实验目标
 *   → day26_edu_tool: 工具友好
 *   → day27_edu_loop: 循环稳定性
 */
#define DAY27_DRV_NAME          "day27_edu_loop"   /* PCI 驱动名（在 /sys/bus/pci/drivers/ 下） */
#define DAY27_CLASS_NAME        "day27_edu"        /* sysfs class 名（在 /sys/class/ 下） */
#define DAY27_DEV_NAME_FMT      "day27_edu%d"      /* 设备节点名格式（%d = 次设备号） */

/*
 * ==================== 第2部分：EDU 设备 PCI ID ====================
 *
 * 与 Day26 完全相同：
 *   Vendor ID:  0x1234（QEMU 的虚拟厂商 ID）
 *   Device ID:  0x11e8（EDU 教学设备 ID）
 *
 * 【为什么 EDU 设备 ID 不变？】
 *   → EDU 设备是 QEMU 模拟的，不是真实硬件
 *   → 1234:11e8 是 QEMU 官方分配给 EDU 教学设备的 ID
 *   → 所有 EDU 相关实验（day25~day27）都使用相同的设备 ID
 */
#define DAY27_EDU_VENDOR_ID     0x1234
#define DAY27_EDU_DEVICE_ID     0x11e8

/*
 * ==================== 第3部分：EDU BAR0 寄存器布局 ====================
 *
 * 【与 Day26 的区别】
 *   Day26 定义了完整的寄存器列表（包括 ID、LIVENESS 等）
 *   Day27 只定义中断相关寄存器，更精简
 *
 * Day27 用到的寄存器：
 *   IRQ_STATUS (0x24)：读取中断状态
 *   IRQ_RAISE (0x60)：写入触发中断
 *   IRQ_ACK (0x64)：写入清除中断
 *
 * Day27 没用到但 Day26 用到的寄存器：
 *   IDENTITY (0x00)：Day26 probe 时读取验证
 *   LIVENESS (0x04)：Day26 probe 时读取验证
 *   STATUS (0x20)：Day26 用到，Day27 没用
 *
 * 【为什么简化？】
 *   → 200 次循环中，每轮都读 ID/LIVENESS 是多余的
 *   → 这些字段的验证价值在 Day26 已体现，Day27 专注稳定性
 */

/*
 * EDU MMIO 寄存器布局（Day27 仅用中断相关寄存器）：
 *
 * 偏移      名称           读写属性    说明
 * ──────────────────────────────────────────────────────────
 * 0x24     IRQ_STATUS     只读       当前中断状态（位掩码）
 * 0x60     IRQ_RAISE      只写       写入任意值触发 MSI 中断
 * 0x64     IRQ_ACK        只写       写入 IRQ_STATUS 的值清除中断
 */
#define DAY27_EDU_REG_IDENTITY      0x00   /* 设备ID（Day27 不用） */
#define DAY27_EDU_REG_LIVENESS      0x04   /* 存活验证（Day27 不用） */
#define DAY27_EDU_REG_IRQ_STATUS    0x24   /* 中断状态（只读，位掩码） */
#define DAY27_EDU_REG_IRQ_RAISE     0x60   /* 触发中断（只写） ← 核心！ */
#define DAY27_EDU_REG_IRQ_ACK       0x64   /* 清除中断（只写） */

/*
 * ==================== 第4部分：day27_dev 结构体 ====================
 *
 * 【与 Day26 day26_dev 的区别】
 *
 * Day26 结构体（有 11 个字段）：
 *   - identity_value: ID 寄存器值
 *   - liveness_value: LIVENESS 测试值
 *   - liveness_inverted: LIVENESS 取反值
 *
 * Day27 结构体（精简到 10 个字段）：
 *   - 去掉 identity_value（不再 probe 时读取）
 *   - 去掉 liveness_value 和 liveness_inverted（不再 probe 时读取）
 *
 * 【为什么精简？】
 *   → 200 次循环中，这些字段没有实际作用
 *   → 简化后的结构体更小，分配/释放更快
 *   → 代码更少，Bug 更少
 *
 * 结构体包含三类成员：
 *   1. PCI 资源：pdev、bar0、bar0_start、bar0_len
 *   2. 中断资源：irq_vector、irq_count、last_irq_status、last_ack_value、irq_lock
 *   3. 字符设备资源：devt、cdev、device
 */
struct day27_dev {
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
    unsigned int irq_vector;       /* Linux 内核分配的中断向量号（IRQ 号） */
                                   /* 用于 request_irq() 和 free_irq() */
    u64 irq_count;                 /* 中断处理函数被调用的次数（handler 中自增）*/

    u32 last_irq_status;           /* 最近一次中断的 IRQ_STATUS 值（handler 中记录）*/
    u32 last_ack_value;            /* 最近一次清除中断时写入 IRQ_ACK 的值 */

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

    /*
     * 【Day26 vs Day27 结构体对比】
     *
     * Day26 多出来的字段（Day27 已去掉）：
     *     u32 identity_value;          // ID 寄存器值
     *     u32 liveness_value;         // LIVENESS 测试值
     *     u32 liveness_inverted;       // LIVENESS 取反值
     *
     * 为什么去掉？
     *   → Day26 在 probe 时读取这些值用于验证 MMIO 映射正确
     *   → Day27 假设链路已打通（Day26 已验证），专注稳定性
     *   → 200 次循环中这些字段没有作用，精简掉更稳定
     */
};

/*
 * 为什么没有包含 ioctl 编号定义？
 *   ioctl 编号定义在 day27_edu_uapi.h 中
 *   因为 ioctl 编号需要被用户态程序 include
 *   uapi 头文件被设计成"用户态和驱动共享"
 */

#endif /* DAY27_EDU_LOOP_H */
