// SPDX-License-Identifier: GPL-2.0
/*
 * day24_ivshmem_mmio.c - ivshmem BAR2 共享内存协议驱动
 *
 * ==================== 代码框架总览 ====================
 *
 *  ┌─────────────────────────────────────────────────────────────┐
 *  │                    PCI 驱动 + misc 字符设备                  │
 *  │                                                             │
 *  │  1. pci_device_id       ──► 匹配 ivshmem-plain (1af4:1110) │
 *  │  2. probe / remove      ──► PCI 设备生命周期管理              │
 *  │  3. BAR0/BAR2 映射      ──► pci_iomap() 物理地址→虚拟地址   │
 *  │  4. 共享内存协议头       ──► BAR2 起始处自定义协议格式        │
 *  │  5. miscdevice          ──► 简化为字符设备自动创建节点       │
 *  │  6. file_operations    ──► open/read/write/ioctl/llseek   │
 *  └─────────────────────────────────────────────────────────────┘
 *
 * ==================== 学习重点 ====================
 *
 *  day24 在 day23（PCI 资源接管）的基础上更进一步：
 *  - 不再只读一个 BAR0 dword 验证映射成功
 *  - 而是在 BAR2 共享内存上建立一套最小可验证的通信协议
 *  - 通过 miscdevice 暴露给用户态
 *
 *  核心问题：驱动和用户态如何"理解"同一块共享内存？
 *  答案：在共享内存起始位置定义固定格式的"协议头"
 *
 *  阶段分工：
 *    day22 = 设备发现 + PCI 骨架
 *    day23 = PCI 资源接管（enable / request_regions / iomap）
 *    day24 = BAR2 共享内存协议 + miscdevice 用户态接口
 *    day25 = MSI 中断（doorbell）
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "include/day24_ivshmem_mmio.h"

/*
 * ==================== 第1步：定义支持的设备列表 ====================
 *
 * pci_device_id 数组告诉内核：
 *   "当枚举到 vendor=0x1af4, device=0x1110 时，调用我这个驱动的 probe"
 *
 * ivshmem 有两种设备：
 *   1af4:1110 (ivshmem-plain) ──► 只有共享内存，无 doorbell 中断
 *   1af4:1111 (ivshmem-doorbell) ──► 共享内存 + MSI doorbell 中断
 *
 * day24 使用 plain 设备（中断在 day25 再处理）
 *
 * MODULE_DEVICE_TABLE 让内核在模块加载时导出 id 列表到 sysfs，
 * PCI bus 枚举时会自动匹配到本驱动
 */
static const struct pci_device_id day24_pci_ids[] = {
    { PCI_DEVICE(DAY24_IVSHMEM_VENDOR_ID, DAY24_IVSHMEM_DEVICE_ID) },
    //        = PCI_DEVICE(0x1af4, 0x1110)  ← ivshmem-plain
    { 0, }  /* 结束标记 */
};
MODULE_DEVICE_TABLE(pci, day24_pci_ids);

/*
 * ==================== 第2步：BAR 过滤函数 ====================
 *
 * 并非所有 BAR 都值得映射：
 *   BAR 长度 = 0  → 设备没有这个 BAR（未使用）
 *   BAR 不是 IORESOURCE_MEM → 是 IO port 或未分配
 *
 * 这个过滤器保证只映射真实存在的、类型为内存的 BAR
 *
 * 为什么需要防护？
 *   ivshmem-plain 的 BAR1 通常不存在（len=0），
 *   如果不检查就调用 pci_iomap(pdev, 1)，会返回 NULL
 */
static bool day24_bar_should_map(struct pci_dev *pdev, int bar)
{
    resource_size_t len = pci_resource_len(pdev, bar);
    unsigned long flags = pci_resource_flags(pdev, bar);

    if (!len)
        return false;  // BAR 不存在（未分配或长度为 0）

    if (!(flags & IORESOURCE_MEM))
        return false;  // 不是内存映射 I/O，跳过

    return true;
}

/*
 * ==================== 第3步：读取并打印 BAR 资源信息 ====================
 *
 * pci_resource_* 是 Linux 内核标准 API：
 *   pci_resource_start()  ──► 获取 BAR 起始物理地址
 *   pci_resource_end()   ──► 获取 BAR 结束物理地址
 *   pci_resource_len()   ──► 获取 BAR 长度
 *   pci_resource_flags() ──► 获取 BAR 属性（IORESOURCE_MEM / IORESOURCE_IO）
 *
 * 注意：这里只是"读取" PCI 配置空间中的值，还没有做 iomap 映射
 *       %pa 是 resource_size_t 的专用格式化符，自动处理 32/64 位差异
 */
static void day24_dump_bar(struct pci_dev *pdev, struct day24_dev *d, int bar)
{
    struct day24_bar_info *bi = &d->bar[bar];

    bi->index = bar;
    bi->start = pci_resource_start(pdev, bar);
    bi->end   = pci_resource_end(pdev, bar);
    bi->len   = pci_resource_len(pdev, bar);
    bi->flags = pci_resource_flags(pdev, bar);

    dev_info(&pdev->dev,
             "BAR%d: start=%pa end=%pa len=%pa flags=0x%lx\n",
             bar, &bi->start, &bi->end, &bi->len, bi->flags);
}

/*
 * ==================== 第4步：解除 BAR 映射 ====================
 *
 * probe 中对哪些 BAR 调用了 pci_iomap，remove 中就必须对应调用 pci_iounmap
 *
 * 注意：
 *   - 不存在的 BAR 的 vaddr 是 NULL，pci_iounmap(NULL) 是安全的（Linux 内部处理）
 *   - 这是逆序操作：最后映射的最先解除
 */
static void day24_unmap_bars(struct day24_dev *d)
{
    int i;

    for (i = 0; i < PCI_STD_NUM_BARS; i++) {
        if (!d->bar[i].vaddr)
            continue;  // 没映射过的跳过
        pci_iounmap(d->pdev, d->bar[i].vaddr);
        d->bar[i].vaddr = NULL;  // 防止重复释放
    }
}

/*
 * ==================== 第5步：映射 BAR 到虚拟地址 ====================
 *
 * pci_iomap(pdev, bar, 0) 的三个参数：
 *   参数1：pci_dev ──► 设备指针
 *   参数2：bar     ──► BAR 编号（0~5）
 *   参数3：0       ──► 映射长度，0 表示映射整个 BAR（不是只映射前 0 字节）
 *
 * 返回值：
 *   成功 ──► void __iomem * 虚拟地址（CPU 可访问的指针）
 *   失败 ──► NULL
 *
 * 为什么返回 void __iomem *？
 *   __iomem 是 Linux 内核的"IO 内存"标记：
 *   - 告知编译器这是 MMIO 地址，不是普通内存
 *   - 防止编译器将 readl/writel 优化成普通内存访问
 *   - 必须用 readl()/writel() 系列函数访问，不能用 *ptr 解引用
 */
static int day24_map_bar(struct day24_dev *d, int bar)
{
    void __iomem *vaddr;

    if (!day24_bar_should_map(d->pdev, bar))
        return 0;  // 不需要映射的 BAR，跳过（不是错误）

    vaddr = pci_iomap(d->pdev, bar, 0);
    if (!vaddr) {
        dev_err(&d->pdev->dev, "BAR%d: pci_iomap failed\n", bar);
        return -ENOMEM;
    }

    d->bar[bar].vaddr = vaddr;
    dev_info(&d->pdev->dev, "BAR%d mapped: vaddr=%p\n", bar, vaddr);
    return 0;
}

/*
 * ==================== 第6步：BAR2 地址计算 ====================
 *
 * BAR2 是共享内存窗口，其虚拟地址 + 偏移量 = 实际访问地址
 *
 * 为什么需要单独函数？
 *   协议头的偏移量都是相对于 BAR2 起始地址的常量
 *   封装成 inline 函数后，所有协议操作统一调用，便于维护
 *
 * 内存布局（三层寻址）：
 *   d->bar[2].vaddr  ──► BAR2 基地址（pci_iomap 返回的虚拟地址）
 *   + off            ──► 协议偏移（0x00~0x20 是协议头，0x20+ 是 payload）
 *   = 实际 MMIO 地址  ──► readl/writel 的目标
 */
static inline void __iomem *day24_bar2_ptr(struct day24_dev *d, u32 off)
{
    return d->bar[2].vaddr + off;
}

/*
 * ==================== 第7步：协议头读写（readl/writel） ====================
 * 从 BAR2 协议头读取 32-bit 字段
 * 为什么协议头用 readl/writel，而不是 memcpy？
 *   协议头字段都是 4 字节对齐的单个 32-bit 值：
 *     MAGIC(4B) / VERSION(4B) / SEQ(4B) / STATE(4B) / LEN(4B)
 *
 *   readl/writel 的特性：
 *     - 保证 32-bit 原子访问（不会出现"半写入"）
 *     - 生成明确的 MMIO 存储/加载指令
 *     - 不会被编译器优化成栈操作或缓存
 *
 * 为什么不用 memcpy_toio/fromio 读写协议头？
 *   memcpy_toio(fromio) 设计用于大块数据批量传输，
 *   对单个 4 字节字段用 memcpy 效率低且语义不清
 */
static u32 day24_proto_read32(struct day24_dev *d, u32 off)
{
    return readl(day24_bar2_ptr(d, off));
}

static void day24_proto_write32(struct day24_dev *d, u32 off, u32 val)
{
    writel(val, day24_bar2_ptr(d, off));
}

/*
 * ==================== 第8步：计算 payload 容量 ====================
 *
 * BAR2 总长度 - 协议头长度 = 可用于实际数据的容量
 *
 * BAR2 总长度 = 4MB (0x400000)
 * 协议头长度 = DAY24_PROTO_PAYLOAD_OFF = 0x20 (32 字节)
 * 最大 payload 容量 = 4MB - 32 字节
 *
 * DAY24_PROTO_MAX_PAYLOAD (256) 是应用层的人为限制，
 * 防止用户态一次读写太多（实际可用的共享内存远大于 256B）
 */
static size_t day24_payload_capacity(struct day24_dev *d)
{
    if (d->bar[2].len <= DAY24_PROTO_PAYLOAD_OFF)
        return 0;  // BAR2 太短，没有 payload 区
    return min_t(size_t,
                 (size_t)(d->bar[2].len - DAY24_PROTO_PAYLOAD_OFF),
                 DAY24_PROTO_MAX_PAYLOAD);
}

/*
 * ==================== 第9步：协议初始化（init-if-needed） ====================
 *
 * 核心设计：只在协议头"不存在"时才写入初始值
 *
 * 为什么需要这个逻辑？
 *
 *   场景 A：QEMU 重启，BAR2 物理内存内容被清零
 *     → magic != DAY24_PROTO_MAGIC → 重新初始化 ✓
 *       insmod → probe → init_if_needed → 发现 magic=0 → 重新初始化
 *   场景 B：BAR2 是文件-backed 的共享内存后端
 *     → 重启后内容可能还在，不重复写入 ✓
 *
 *   场景 C：驱动 rmmod 后重新 insmod
 *     → BAR2 物理内存内容不变
 *     → 驱动重新映射，得到新的虚拟地址
 *     → magic 还在，不重复写入，保持协议状态持久性 ✓
 *
 * 为什么不用"每次 probe 都重新初始化"？
 *   因为共享内存是跨进程的：
 *     - 驱动侧和用户态侧都访问同一块 BAR2
 *     - 驱动重启不应该覆盖用户态已写入的数据
 *
 * memset_io vs memset：
 *   memset_io 生成 MMIO 存储指令（对总线发起的访问）
 *   memset     可能被编译器优化成对 CPU 缓存的操作
 *   对 MMIO 区域必须用 memset_io，否则可能写出错
 */
static void day24_proto_init_if_needed(struct day24_dev *d)
{
    u32 magic;
    u32 version;
    size_t cap = day24_payload_capacity(d);

    if (!d->bar[2].vaddr)
        return;  // BAR2 还没映射（不应该发生）

    /*
     * ========== 读取现有协议头 ==========
     */
    magic = day24_proto_read32(d, DAY24_PROTO_OFF_MAGIC);    // 0x00
    version = day24_proto_read32(d, DAY24_PROTO_OFF_VERSION); // 0x04

    /*
     * ========== 检查是否已初始化 ==========
     * 如果 magic 和 version 都匹配，说明协议头已存在，跳过初始化
     */
    if (magic == DAY24_PROTO_MAGIC && version == DAY24_PROTO_VERSION) {
        dev_info(&d->pdev->dev,
                 "protocol header exists: magic=0x%08x version=%u seq=%u state=%u len=%u\n",
                 magic,
                 version,
                 day24_proto_read32(d, DAY24_PROTO_OFF_SEQ),
                 day24_proto_read32(d, DAY24_PROTO_OFF_STATE),
                 day24_proto_read32(d, DAY24_PROTO_OFF_LEN));
        return;  // ← 关键：已初始化则直接返回，不覆盖
    }

    /*
     * ========== 首次初始化：写入所有协议头字段 ==========
     *   MAGIC   = 0x44593234 ("DY24" 小端序)
     *   VERSION = 1
     *   SEQ     = 0
     *   STATE   = DAY24_STATE_READY (1)
     *   LEN     = 0
     */
    day24_proto_write32(d, DAY24_PROTO_OFF_MAGIC, DAY24_PROTO_MAGIC);
    day24_proto_write32(d, DAY24_PROTO_OFF_VERSION, DAY24_PROTO_VERSION);
    day24_proto_write32(d, DAY24_PROTO_OFF_SEQ, 0);
    day24_proto_write32(d, DAY24_PROTO_OFF_STATE, DAY24_STATE_READY);
    day24_proto_write32(d, DAY24_PROTO_OFF_LEN, 0);

    if (cap)
        memset_io(day24_bar2_ptr(d, DAY24_PROTO_PAYLOAD_OFF), 0, cap);
        // 清零整个 payload 区（从 0x20 开始到 BAR2 末尾）

    dev_info(&d->pdev->dev,
             "protocol header initialized: magic=0x%08x version=%u payload_cap=%zu\n",
             DAY24_PROTO_MAGIC, DAY24_PROTO_VERSION, cap);
}

/*
 * ==================== 第10步：MMIO 写入白名单 ====================
 *
 * 为什么需要白名单？
 *   用户态通过 ioctl(MMIO_WRITE32) 可以指定任意偏移和值
 *   如果不限制，可以把 MAGIC / VERSION 也改掉，破坏协议
 *
 * 白名单策略：
 *   ✓ 可以写：SEQ (0x08) / STATE (0x0c) / LEN (0x10)
 *   ✗ 不能写：MAGIC (0x00) / VERSION (0x04) / reserved 区域
 *
 * 为什么 LEN 也可以写？
 *   因为 write() 系统调用会更新 LEN，
 *   用户态如果想自己管理长度（绕过 write），需要这个权限
 *
 * 为什么 MAGIC / VERSION 不能写？
 *   这两个字段是协议的"身份标识"
 *   如果被改掉，init-if-needed 会认为协议被破坏
 *   唯一恢复方式是重新初始化（丢失所有数据）
 */
static bool day24_mmio_offset_allowed(u32 off)
{
    switch (off) {
    case DAY24_PROTO_OFF_SEQ:    // 0x08  ✓
    case DAY24_PROTO_OFF_STATE:  // 0x0c  ✓
    case DAY24_PROTO_OFF_LEN:    // 0x10  ✓
        return true;
    default:
        return false;  // MAGIC(0x00)/VERSION(0x04)/reserved 不可写
    }
}

/*
 * ==================== 第11步：file_operations - open ====================
 *
 * open() 的核心任务：建立 file->private_data 关联
 *
 * 用户态调用 open("/dev/day24_ivshmem0") 后：
 *   1. VFS 根据设备号找到本驱动的 inode
 *   2. inode->i_rdev 包含主设备号和次设备号
 *   3. 内核根据主设备号找到 miscdevice 结构体
 *   4. 调用本驱动的 open()
 *
 * file->private_data 的作用：
 *   驱动私有数据（day24_dev*）存到这里，
 *   后续 read/write/ioctl 都能通过 file->private_data 拿到
 *
 * container_of 宏的原理：
 *   已知结构体成员地址（misc），求嵌该成员的结构体基地址（day24_dev）
 *   实现：((type *)((char *)member - offsetof(type, member)))
 *   这是 Linux 内核中反向推导结构体指针的标准做法
 */
static int day24_open(struct inode *inode, struct file *file)
{
    struct miscdevice *misc = file->private_data;
    struct day24_dev *d = container_of(misc, struct day24_dev, miscdev);
    // container_of：已知 misc 在 day24_dev 中的偏移，反推 day24_dev 指针
    // 场景：file->private_data 指向 miscdevice 成员，misc 嵌在 d 里

    file->private_data = d;
    // 把 d（day24_dev*）存到 file 里，后续操作直接取用
    return 0;
}

/*
 * ==================== 第12步：file_operations - llseek ====================
 *
 * llseek 决定 read/write 的偏移基准
 *
 * whence 三种模式：
 *   SEEK_SET (0) ──► off 是绝对位置，从 0 开始算
 *   SEEK_CUR (1) ──► off 是相对位置，从当前 f_pos 算
 *   SEEK_END (2) ──► off 是相对位置，从文件末尾算（通常为负）
 *
 * 文件位置合法性限制：
 *   [0, payload_capacity] 是合法区间
 *   不能 < 0（文件头之前）
 *   不能 > payload_capacity（超出 payload 区）
 *
 * 为什么 llseek 对共享内存很重要？
 *   因为 read/write 操作的是 BAR2 的 payload 区，
 *   llseek 决定了从 payload 的哪个位置开始读/写
 */
static loff_t day24_llseek(struct file *file, loff_t off, int whence)
{
    struct day24_dev *d = file->private_data;
    loff_t newpos;
    loff_t limit = (loff_t)day24_payload_capacity(d);  // 最大偏移 = payload 容量

    switch (whence) {
    case SEEK_SET:
        newpos = off;                           // 从头算
        break;
    case SEEK_CUR:
        newpos = file->f_pos + off;             // 从当前位置算
        break;
    case SEEK_END:
        newpos = limit + off;                   // 从末尾算（off 通常为负）
        break;
    default:
        return -EINVAL;  // 无效的 whence
    }

    if (newpos < 0 || newpos > limit)
        return -EINVAL;  // 越界，拒绝

    file->f_pos = newpos;
    return newpos;  // 返回新的文件位置
}

/*
 * ==================== 第13步：file_operations - read ====================
 * read 是指：用户态调用 read() 系统调用，驱动从 BAR2 读取数据返回给用户态
 * read() = 用户态进程调用 read() → 驱动从 BAR2 的 payload 区读取，返回给用户态
 * read() 的数据流（三次复制）：
 *
 *   BAR2 MMIO ──► kbuf(内核栈) ──► buf(用户态)
 *      第一次复制                 第二次复制
 *   (memcpy_fromio)           (copy_to_user)
 *
 * 为什么不能直接 BAR2 → 用户态？
 *   因为 MMIO 访问和用户态内存访问是不同的 CPU 操作
 *   中间必须经过内核栈作为中转
 *
 * 协议头 LEN 字段的作用：
 *   LEN 记录了"当前有多少有效数据"
 *   read 不能超过 LEN，即使 BAR2 payload 区更大
 *
 * 返回值语义：
 *   > 0 ──► 成功读取的字节数
 *   = 0 ──► EOF（已读到 LEN 长度，或 *ppos >= payload_len）
 *   < 0 ──► 错误码（-ENOMEM / -EFAULT / -ENODEV）
 */
static ssize_t day24_read(struct file *file, char __user *buf,
                           size_t count, loff_t *ppos)
{
    struct day24_dev *d = file->private_data;
    size_t payload_len;   // 有效数据长度（从协议头 LEN 字段读）
    size_t cap;           // payload 总容量
    size_t to_copy;       // 本次实际要复制多少
    void *kbuf;
    int rc = 0;

    if (!d->bar[2].vaddr)
        return -ENODEV;  // BAR2 未映射

    mutex_lock(&d->lock);

    /*
     * ========== 第1步：确定有效数据范围 ==========
     */
    cap = day24_payload_capacity(d);  // BAR2 总长 - 0x20
    payload_len = day24_proto_read32(d, DAY24_PROTO_OFF_LEN);
    if (payload_len > cap)             // 防脏数据：LEN 不能超过容量
        payload_len = cap;

    if (*ppos >= payload_len)         // 已经读完了，返回 EOF
        goto out_zero;

    /*
     * ========== 第2步：计算本次复制量 ==========
     * count      = 用户态想要读的字节数
     * payload_len - *ppos = 剩余可读的字节数
     * 取两者较小值
     */
    to_copy = min_t(size_t, count, payload_len - (size_t)*ppos);

    /*
     * ========== 第3步：MMIO → 内核栈 ==========
     */
    kbuf = kmalloc(to_copy, GFP_KERNEL);
    if (!kbuf) {
        rc = -ENOMEM;
        goto out_unlock;
    }

    memcpy_fromio(kbuf,                                    // 目标：内核栈
                 day24_bar2_ptr(d, DAY24_PROTO_PAYLOAD_OFF + (u32)*ppos),
                 to_copy);                                // 长度

    /*
     * ========== 第4步：内核栈 → 用户态 ==========
     */
    if (copy_to_user(buf, kbuf, to_copy)) {
        kfree(kbuf);
        rc = -EFAULT;
        goto out_unlock;
    }

    kfree(kbuf);
    *ppos += to_copy;   // 更新文件位置，下一次 read 从这里继续
    rc = (int)to_copy;
    goto out_unlock;

out_zero:
    rc = 0;  // EOF 返回 0（不是错误）
out_unlock:
    mutex_unlock(&d->lock);
    return rc;
}

/*
 * ==================== 第14步：file_operations - write ====================
 * write 是指：用户态调用 write() 系统调用，驱动把数据写入 BAR2 (MMIO)
 * write("hello") = 用户态进程调用 write() → 驱动把 "hello" 写入 BAR2 的 payload 区
 * write() 的数据流（三次复制 + 协议头更新）：
 *
 *   用户态 buf ──► kbuf(内核栈) ──► BAR2 MMIO
 *      第一次复制              第二次复制
 *   (memdup_user)          (memcpy_toio)
 *
 * 写入成功后协议头更新（原子性）：
 *   LEN  = max(旧LEN, *ppos + to_copy) ──► 更新为新的数据长度
 *   STATE = DAY24_STATE_USER_WRITTEN   ──► 标记为"有用户数据"
 *   SEQ  ++                               ──► 序列号自增，记录写入历史
 *
 * 为什么需要 mutex？
 *   如果多个进程同时 write，协议头可能被并发改坏：
 *     - 进程 A 读到 LEN=5，准备更新
 *     - 进程 B 同时写到不同位置
 *     - 进程 A 的更新覆盖了进程 B 的 LEN
 *   mutex 保证同一时刻只有一个进程修改协议头
 */
static ssize_t day24_write(struct file *file, const char __user *buf,
                            size_t count, loff_t *ppos)
{
    struct day24_dev *d = file->private_data;
    size_t cap;
    size_t to_copy;
    size_t new_len;
    void *kbuf;
    u32 seq;
    int rc = 0;

    if (!d->bar[2].vaddr)
        return -ENODEV;

    mutex_lock(&d->lock);

    cap = day24_payload_capacity(d);
    if (*ppos >= cap) {       // 超出容量
        rc = -ENOSPC;
        goto out_unlock;
    }

    /*
     * ========== 第1步：用户态 → 内核栈 ==========
     */
    to_copy = min_t(size_t, count, cap - (size_t)*ppos);
    kbuf = memdup_user(buf, to_copy);  // 分配内核内存并拷贝用户数据
    if (IS_ERR(kbuf)) {
        rc = PTR_ERR(kbuf);
        goto out_unlock;
    }

    /*
     * ========== 第2步：内核栈 → BAR2 MMIO ==========
     */
    memcpy_toio(day24_bar2_ptr(d, DAY24_PROTO_PAYLOAD_OFF + (u32)*ppos),
                kbuf, to_copy);
    kfree(kbuf);  // 立即释放内核临时内存

    /*
     * ========== 第3步：更新文件位置 ==========
     */
    *ppos += to_copy;

    /*
     * ========== 第4步：更新协议头（原子性保护） ==========
     * 三步一起完成才算成功：
     *   ① LEN 更新为 max(旧值, 新偏移)
     *   ② STATE = USER_WRITTEN
     *   ③ SEQ++
     */
    new_len = max_t(size_t,
                    day24_proto_read32(d, DAY24_PROTO_OFF_LEN),
                    (size_t)*ppos);
    day24_proto_write32(d, DAY24_PROTO_OFF_LEN, (u32)new_len);
    day24_proto_write32(d, DAY24_PROTO_OFF_STATE, DAY24_STATE_USER_WRITTEN);
    seq = day24_proto_read32(d, DAY24_PROTO_OFF_SEQ) + 1;
    day24_proto_write32(d, DAY24_PROTO_OFF_SEQ, seq);

    dev_info(&d->pdev->dev,
             "payload write: count=%zu new_len=%zu seq=%u\n",
             to_copy, new_len, seq);

    rc = (int)to_copy;
out_unlock:
    mutex_unlock(&d->lock);
    return rc;
}

/*
 * ==================== 第15步：file_operations - ioctl ====================
 *
 * ioctl 是驱动与用户态交互的"控制通道"，专门用来读写协议头字段和管理 payload 状态
 * 与 read/write 不同，ioctl 传递的是"命令 + 数据结构"
 *
 * 为什么要用 ioctl 而不是 read/write？
 *   - GET_INFO 需要返回大量结构化数据（struct day24_info_uapi）
 *   - MMIO_READ/WRITE 需要同时传递偏移量和值
 *   - CLEAR_PAYLOAD 只需要一个命令，不需要传递数据
 *
 * ioctl nr（命令号）的设计：
 *   Linux ioctl 采用 (dir << 30) | (size << 16) | (nr << 0) 编码
 *   _IOR / _IOWR / _IOW 宏自动处理这个编码，用户态无需关心
 *
 *   DAY24_IOC_GET_INFO      (_IOR)  ──► 用户态读取，不修改驱动数据
 *   DAY24_IOC_MMIO_READ32   (_IOWR) ──► 用户态传入偏移，驱动返回数值
 *   DAY24_IOC_MMIO_WRITE32  (_IOW)  ──► 用户态传入偏移+数值，驱动写入
 *   DAY24_IOC_CLEAR_PAYLOAD (_IO)   ──► 无参数命令，清空 payload
 *
 * 错误处理统一模式：
 *   copy_from_user ──► 检查返回值，非0返回 -EFAULT
 *   边界检查        ──► 返回 -EINVAL
 *   copy_to_user   ──► 检查返回值，非0返回 -EFAULT
 */
static long day24_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct day24_dev *d = file->private_data;
    struct day24_info_uapi info;
    struct day24_mmio32_uapi mmio;
    u32 seq;
    long rc = 0;

    if (!d->bar[2].vaddr)
        return -ENODEV;

    mutex_lock(&d->lock);

    switch (cmd) {

    /*
     * ========== DAY24_IOC_GET_INFO ==========
     * 用途：用户态获取驱动的完整状态快照
     *
     * 包括：
     *   - PCI 信息（vendor/device）
     *   - BAR0 首 dword（仅记录，用于 info）
     *   - 协议头 5 个字段（magic/version/seq/state/len）
     *   - BAR0 和 BAR2 的地址/长度/flags
     *
     * 使用场景：调试时快速查看所有状态
     */
    case DAY24_IOC_GET_INFO:
        memset(&info, 0, sizeof(info));
        info.vendor = d->pdev->vendor;
        info.device = d->pdev->device;
        info.bar0_first_dword = d->bar0_first_dword;
        info.proto_magic = day24_proto_read32(d, DAY24_PROTO_OFF_MAGIC);
        info.proto_version = day24_proto_read32(d, DAY24_PROTO_OFF_VERSION);
        info.proto_seq = day24_proto_read32(d, DAY24_PROTO_OFF_SEQ);
        info.proto_state = day24_proto_read32(d, DAY24_PROTO_OFF_STATE);
        info.proto_payload_len = day24_proto_read32(d, DAY24_PROTO_OFF_LEN);

        info.bar0.index = 0;
        info.bar0.start = d->bar[0].start;
        info.bar0.end   = d->bar[0].end;
        info.bar0.len   = d->bar[0].len;
        info.bar0.flags = d->bar[0].flags;

        info.bar2.index = 2;
        info.bar2.start = d->bar[2].start;
        info.bar2.end   = d->bar[2].end;
        info.bar2.len   = d->bar[2].len;
        info.bar2.flags = d->bar[2].flags;

        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            rc = -EFAULT;
        break;

    /*
     * ========== DAY24_IOC_MMIO_READ32 ==========
     * 用途：读取 BAR2 指定偏移的 32-bit 值
     *
     * 三重边界检查：
     *   ① (offset & 0x3) ──► 必须 4 字节对齐
     *   ② offset >= PAYLOAD_OFF ──► 不能在 payload 区（只读协议头）
     *   ③ offset + 4 > bar2.len ──► 不能超出 BAR2 总长
     */
    case DAY24_IOC_MMIO_READ32:
        if (copy_from_user(&mmio, (void __user *)arg, sizeof(mmio))) {
            rc = -EFAULT;
            break;
        }
        if ((mmio.offset & 0x3) ||
            mmio.offset >= DAY24_PROTO_PAYLOAD_OFF ||
            mmio.offset + sizeof(u32) > d->bar[2].len) {
            rc = -EINVAL;
            break;
        }
        mmio.value = day24_proto_read32(d, mmio.offset);
        if (copy_to_user((void __user *)arg, &mmio, sizeof(mmio)))
            rc = -EFAULT;
        break;

    /*
     * ========== DAY24_IOC_MMIO_WRITE32 ==========
     * 用途：写入 BAR2 指定偏移的 32-bit 值
     *
     * 与 READ32 的区别：
     *   - 多一层白名单检查（day24_mmio_offset_allowed）
     *   - 写入后 SEQ++
     *
     * 为什么写入要更新 SEQ？
     *   因为任何协议头修改都应该被记录，
     *   SEQ 是修改次数的计数器，便于追踪状态变化历史
     */
    case DAY24_IOC_MMIO_WRITE32:
        if (copy_from_user(&mmio, (void __user *)arg, sizeof(mmio))) {
            rc = -EFAULT;
            break;
        }
        if (!day24_mmio_offset_allowed(mmio.offset) ||
            mmio.offset + sizeof(u32) > d->bar[2].len) {
            rc = -EINVAL;
            break;
        }
        day24_proto_write32(d, mmio.offset, mmio.value);
        seq = day24_proto_read32(d, DAY24_PROTO_OFF_SEQ) + 1;
        day24_proto_write32(d, DAY24_PROTO_OFF_SEQ, seq);
        dev_info(&d->pdev->dev,
                 "MMIO write: offset=0x%08x value=0x%08x seq=%u\n",
                 mmio.offset, mmio.value, seq);
        break;

    /*
     * ========== DAY24_IOC_CLEAR_PAYLOAD ==========
     * 用途：重置 payload 区到初始状态
     *
     * 执行：
     *   ① memset_io(payload区, 0, 容量) ──► 清零数据
     *   ② LEN = 0                       ──► 长度为 0
     *   ③ STATE = EMPTY                  ──► 状态为空
     *   ④ SEQ++                          ──► 序列号自增
     *
     * 注意：不清协议头本身（magic/version），只清 payload
     */
    case DAY24_IOC_CLEAR_PAYLOAD:
        memset_io(day24_bar2_ptr(d, DAY24_PROTO_PAYLOAD_OFF), 0,
                  day24_payload_capacity(d));
        day24_proto_write32(d, DAY24_PROTO_OFF_LEN, 0);
        day24_proto_write32(d, DAY24_PROTO_OFF_STATE, DAY24_STATE_EMPTY);
        seq = day24_proto_read32(d, DAY24_PROTO_OFF_SEQ) + 1;
        day24_proto_write32(d, DAY24_PROTO_OFF_SEQ, seq);
        dev_info(&d->pdev->dev, "payload cleared: seq=%u\n", seq);
        break;

    default:
        rc = -ENOTTY;  // 未知命令
        break;
    }

    mutex_unlock(&d->lock);
    return rc;
}

/*
 * ==================== file_operations 定义完成 ====================
 *
 * 总结各函数职责：
 *   .open      建立 file->private_data 到 day24_dev 的关联
 *   .llseek    在 payload 容量范围内移动读写游标
 *   .read      从 BAR2 payload 区读取数据（受 LEN 限制）
 *   .write     向 BAR2 payload 区写入数据（更新 LEN/STATE/SEQ）
 *   .unlocked_ioctl  读写协议头字段 / 获取信息 / 清空 payload
 */
static const struct file_operations day24_fops = {
    .owner = THIS_MODULE,
    .open  = day24_open,
    .llseek = day24_llseek,
    .read  = day24_read,
    .write = day24_write,
    .unlocked_ioctl = day24_ioctl,
};

/*
 * ==================== 第16步：probe - 设备插入时调用 ====================
 *
 * 当内核 PCI core 枚举到 1af4:1110 时自动调用
 *
 * ==================== probe 内部分解（资源获取顺序） ====================
 *
 *  Step 1: kzalloc ──► 分配私有数据结构
 *           注意：不用 devm_kzalloc，因为后续资源需要手动管理
 *
 *  Step 2: mutex_init ──► 初始化互斥锁
 *           注意：必须在使用锁之前初始化
 *
 *  Step 3: pci_set_drvdata ──► 建立 pdev ↔ d 双向指针
 *           后续通过 pci_get_drvdata(pdev) 可取出 d
 *
 *  Step 4: pci_enable_device ──► 使能 PCI 设备
 *           激活 BAR 地址解码，之后才能访问 MMIO
 *           失败不能继续，跳到 err_free
 *
 *  Step 5: pci_request_regions ──► 申请 BAR 资源独占访问
 *           声明"这些 BAR 我要用了"，防止和其他驱动冲突
 *           失败跳到 err_disable（需要 disable 设备）
 *
 *  Step 6: pci_set_master ──► 设置为主设备模式
 *           使设备可以发起 DMA 事务（目前不用，但保留）
 *
 *  Step 7: dump_bar ──► 打印 BAR0/BAR2 信息（日志）
 *
 *  Step 8: map_bar(0) / map_bar(2) ──► 映射 BAR0 和 BAR2
 *           BAR0 = 256B 寄存器窗口
 *           BAR2 = 4MB 共享内存
 *           失败跳到 err_regions（需要释放 regions）
 *
 *  Step 9: readl(BAR0) ──► 读 BAR0 第一个 dword
 *           仅验证 MMIO 可访问性，不是真的要用这个值
 *
 *  Step 10: proto_init ──► 初始化共享内存协议（init-if-needed）
 *
 *  Step 11: misc_register ──► 注册 miscdevice
 *            自动创建 /dev/day24_ivshmem0
 *            失败跳到 err_unmap（需要解除 BAR 映射）
 *
 * ==================== 错误处理（逆序） ====================
 *
 *  err_unmap      → day24_unmap_bars (解除 BAR0/BAR2 映射)
 *  err_regions    → pci_release_regions (释放 BAR 资源)
 *  err_disable    → pci_disable_device (关闭设备)
 *  err_free       → kfree (释放私有数据)
 */
static int day24_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day24_dev *d;
    int rc;

    dev_info(&pdev->dev,
             "probe enter: vendor=%04x device=%04x class=0x%06x irq=%u\n",
             pdev->vendor, pdev->device, pdev->class, pdev->irq);
    // class=0x058000 → Memory Controller（ivshmem 的 PCI class）
    // irq=0 → ivshmem-plain 没有 MSI（doorbell 设备才有 irq）

    /*
     * ========== Step 1: 分配私有数据 ==========
     */
    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;  // 内存不足，最简单

    d->pdev = pdev;
    mutex_init(&d->lock);       // ← day24 新增：保护协议头的互斥锁
    pci_set_drvdata(pdev, d);   // 绑定双向指针

    /*
     * ========== Step 2: 使能 PCI 设备 ==========
     */
    rc = pci_enable_device(pdev);
    if (rc) {
        dev_err(&pdev->dev, "pci_enable_device failed: %d\n", rc);
        goto err_free;
    }
    d->device_enabled = true;

    /*
     * ========== Step 3: 申请 BAR 资源独占访问 ==========
     */
    rc = pci_request_regions(pdev, DAY24_DRV_NAME);
    if (rc) {
        dev_err(&pdev->dev, "pci_request_regions failed: %d\n", rc);
        goto err_disable;
    }
    d->regions_claimed = true;

    /*
     * ========== Step 4: 设置为主设备 ==========
     */
    pci_set_master(pdev);

    /*
     * ========== Step 5: 读取并保存 BAR 信息（日志） ==========
     */
    day24_dump_bar(pdev, d, 0);   // BAR0: 256B 寄存器窗口
    day24_dump_bar(pdev, d, 2);   // BAR2: 4MB 共享内存

    /*
     * ========== Step 6: 映射 BAR0 和 BAR2 ==========
     */
    rc = day24_map_bar(d, 0);     // BAR0：256 字节寄存器窗口
    if (rc)
        goto err_regions;

    rc = day24_map_bar(d, 2);     // BAR2：4MB 共享内存
    if (rc)
        goto err_unmap;

    /*
     * ========== Step 7: 验证 BAR0 MMIO 可访问 ==========
     */
    if (d->bar[0].vaddr) {
        d->bar0_first_dword = readl(d->bar[0].vaddr);
        dev_info(&pdev->dev, "BAR0 first dword=0x%08x\n",
                 d->bar0_first_dword);
    }

    /*
     * ========== Step 8: 初始化共享内存协议 ==========
     * 这是 day24 相比 day23 的核心区别：
     * day23 只映射 BAR，day24 在 BAR2 上建立协议
     */
    day24_proto_init_if_needed(d);

    /*
     * ========== Step 9: 注册 misc 字符设备 ==========
     * miscdevice 的优势（详见 docs/04_DEEP_LEARNING.md）：
     *   - 自动分配次设备号（MISC_DYNAMIC_MINOR）
     *   - 自动创建 /dev/day24_ivshmem0
     *   - 自动处理 udev 事件
     *   - 无需 class_create / device_create
     */
    d->miscdev.minor = MISC_DYNAMIC_MINOR;
    d->miscdev.name  = DAY24_DEVICE_NAME;    // "day24_ivshmem0"
    d->miscdev.fops  = &day24_fops;
    d->miscdev.parent = &pdev->dev;          // sysfs 中 PCI 设备是父设备
    rc = misc_register(&d->miscdev);
    if (rc) {
        dev_err(&pdev->dev, "misc_register failed: %d\n", rc);
        goto err_unmap;
    }

    dev_info(&pdev->dev,
             "probe success: device=%s payload_cap=%zu\n",
             DAY24_DEVICE_NAME, day24_payload_capacity(d));
    return 0;

/*
 * ==================== 错误处理（逆序释放） ====================
 *
 * 注意顺序：
 *   unmap       ──► 解除 BAR 映射（最后做的最先解除）
 *   regions     ──► 释放 BAR 资源
 *   disable     ──► 关闭设备
 *   free        ──► 释放私有数据
 */
err_unmap:
    day24_unmap_bars(d);
err_regions:
    if (d->regions_claimed) {
        pci_release_regions(pdev);
        d->regions_claimed = false;
    }
err_disable:
    if (d->device_enabled) {
        pci_disable_device(pdev);
        d->device_enabled = false;
    }
err_free:
    pci_set_drvdata(pdev, NULL);
    kfree(d);
    return rc;
}

/*
 * ==================== 第17步：remove - 设备拔出时调用 ====================
 *
 * remove 的核心理念：完全对称 probe
 *   probe 获取了什么资源，remove 就按逆序释放什么资源
 *
 * ==================== remove 内部分解（逆序） ====================
 *
 *  Step 1: misc_deregister ──► 注销 miscdevice
 *          注意：最先执行，因为 miscdevice 依赖于 BAR 映射存在
 *
 *  Step 2: day24_unmap_bars ──► 解除所有 BAR 映射
 *
 *  Step 3: pci_release_regions ──► 释放 BAR 资源声明
 *           只在 regions_claimed=true 时执行（probe 成功才为 true）
 *
 *  Step 4: pci_disable_device ──► 关闭设备
 *           只在 device_enabled=true 时执行（probe 成功才为 true）
 *
 *  Step 5: kfree ──► 释放私有数据
 *
 * 为什么 probe 成功但某些步骤失败后，某些标志位是 false？
 *   因为错误处理是"跳到对应的 err_label"，
 *   到达 err_label 之前的代码设置的那些标志已经设置，
 *   到达 err_label 之后跳过的步骤还没执行，标志还是 false。
 *   所以 remove 中必须检查标志位，不能盲目释放。
 */
static void day24_remove(struct pci_dev *pdev)
{
    struct day24_dev *d = pci_get_drvdata(pdev);

    dev_info(&pdev->dev, "remove enter\n");

    if (!d)
        return;

    /*
     * ========== Step 1: 注销 miscdevice ==========
     */
    misc_deregister(&d->miscdev);

    /*
     * ========== Step 2: 解除 BAR 映射 ==========
     */
    day24_unmap_bars(d);

    /*
     * ========== Step 3: 释放 BAR 资源 ==========
     */
    if (d->regions_claimed)
        pci_release_regions(pdev);

    /*
     * ========== Step 4: 关闭设备 ==========
     */
    if (d->device_enabled)
        pci_disable_device(pdev);

    /*
     * ========== Step 5: 释放私有数据 ==========
     */
    pci_set_drvdata(pdev, NULL);
    kfree(d);

    dev_info(&pdev->dev, "remove leave\n");
}

/*
 * ==================== 第18步：pci_driver 结构体 ====================
 *
 * 驱动注册到内核的核心结构体
 *
 * 重要字段：
 *   .name      - 驱动名称，用于日志显示（出现在 dmesg 中）
 *   .id_table  - 指向 pci_device_id 数组，内核靠它匹配设备
 *   .probe     - 设备插入回调
 *   .remove    - 设备拔出回调
 *
 * 内核匹配机制：
 *   1. PCI core 枚举总线，发现设备
 *   2. 根据 vendor:device 在 id_table 列表中查找匹配项
 *   3. 匹配成功后调用 probe()
 */
static struct pci_driver day24_pci_driver = {
    .name = DAY24_DRV_NAME,
    .id_table = day24_pci_ids,
    .probe = day24_probe,
    .remove = day24_remove,
};

/*
 * ==================== 第19步：模块入口 ====================
 *
 * module_pci_driver() 宏，等价于：
 *
 *   static int __init day24_init(void)
 *   {
 *       return pci_register_driver(&day24_pci_driver);
 *   }
 *   module_init(day24_init);
 *
 *   static void __exit day24_exit(void)
 *   {
 *       pci_unregister_driver(&day24_pci_driver);
 *   }
 *   module_exit(day24_exit);
 *
 * 注意：
 *   __init 标记 init 函数，加载后可以被内核释放（节省内存）
 *   __exit 标记 exit 函数，在模块卸载时调用
 */
module_pci_driver(day24_pci_driver);

/*
 * ==================== 模块信息 ====================
 */
MODULE_AUTHOR("Richer Wong");
MODULE_DESCRIPTION("day24 ivshmem MMIO lab: BAR2 protocol + misc device + user tool");
MODULE_LICENSE("GPL");

/*
 * ==================== 附录：完整数据流图 ====================
 *
 * 用户态进程                     驱动                         BAR2 内存
 * ─────────────────────────────────────────────────────────────────────
 * insmod
 *   → pci_register_driver()
 *   → 等待 PCI 枚举
 *
 * [QEMU 枚举到 ivshmem 设备 1af4:1110]
 *   → 内核调用 day24_probe()
 *     → pci_enable_device()
 *     → pci_request_regions()
 *     → pci_iomap(BAR0) → bar[0].vaddr
 *     → pci_iomap(BAR2) → bar[2].vaddr
 *     → readl(BAR0) 验证 MMIO
 *     → day24_proto_init_if_needed()
 *       → readl(BAR2+0x00) 检查 magic
 *       → 如果不存在：writel(初始化协议头)
 *         → [DY24|1|0|READY|0|................]
 *     → misc_register()
 *       → /dev/day24_ivshmem0 创建
 *
 * open("/dev/day24_ivshmem0")
 *   → day24_open()
 *     → file->private_data = d
 *
 * ioctl(GET_INFO)
 *   → day24_ioctl(GET_INFO)
 *     → 读所有协议头字段
 *     → copy_to_user(info)
 *
 * ioctl(MMIO_WRITE32, {offset=0x0c, value=3})
 *   → day24_ioctl(MMIO_WRITE32)
 *     → 白名单检查 offset=0x0c (STATE) ✓
 *     → writel(3, BAR2+0x0c)
 *     → SEQ++
 *     → [DY24|1|0|3|0|.................]
 *
 * write("hello", 5)
 *   → day24_write()
 *     → memdup_user("hello")
 *     → memcpy_toio(BAR2+0x20, "hello")
 *     → writel(LEN=5)
 *     → writel(STATE=WRITTEN)
 *     → writel(SEQ++)
 *     → [DY24|1|1|WRITTEN|5|hello......]
 *
 * read(buf, 256)
 *   → day24_read()
 *     → readl(LEN=5)
 *     → memcpy_fromio(buf, BAR2+0x20, 5)
 *     → copy_to_user("hello")
 *
 * rmmod
 *   → day24_remove()
 *     → misc_deregister()
 *     → day24_unmap_bars()
 *     → pci_release_regions()
 *     → pci_disable_device()
 *     → kfree(d)
 */
