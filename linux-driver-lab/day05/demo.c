#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/ioctl.h>

/*
 * Day05 主题：waitqueue + workqueue + 并发与上下文
 *
 * 这一版代码的目标不是追求“功能多”，而是把下面这条最关键的链路讲清楚：
 *
 *   用户态 write()/ioctl(SET)
 *          |
 *          v
 *      驱动只登记请求，不做慢处理
 *          |
 *          v
 *      schedule_work() 提交到 workqueue
 *          |
 *          v
 *      worker 线程异步处理，生成结果
 *          |
 *          v
 *      wake_up_interruptible() 唤醒阻塞在 read() 的进程
 *
 * 同时，这一版还保留了：
 *   1. sysfs: enable / counter
 *   2. debugfs: status / log_level
 *
 * 这样就把前面 day03/day04 的“可观察性”也延续下来了。
 */

#define DEVICE_NAME     "demo"
#define CLASS_NAME      "demo_class"
#define DEBUGFS_DIR     "demo_debug"
#define DEMO_BUF_SIZE   128

/*
 * ioctl 命令定义
 *
 * - SET: 用户态传入一个 int，驱动保存 value，并异步处理
 * - GET: 用户态读回当前 value
 */
#define DEMO_IOC_MAGIC  'k'
#define DEMO_IOCTL_SET  _IOW(DEMO_IOC_MAGIC, 1, int)
#define DEMO_IOCTL_GET  _IOR(DEMO_IOC_MAGIC, 2, int)

struct demo_device {
    /* 字符设备基础成员 */
    dev_t devt;
    struct cdev cdev;
    struct class *class;
    struct device *device;

    /*
     * 并发保护锁
     *
     * read()/write()/ioctl()/workqueue handler 都会访问共享状态，
     * 这些路径都属于“可睡眠上下文”，因此这里使用 mutex。
     *
     * 为什么不用 spinlock？
     * 因为当前实验还没有进入硬中断 top-half，也没有原子上下文的需求。
     * 这里的重点是先把“共享状态保护”与“阻塞/异步处理”学清楚。
     */
    struct mutex lock;

    /*
     * 等待队列头
     *
     * 作用：
     * - 当 read() 发现没有数据可读时，不忙等，而是睡眠在这个等待队列上
     * - 当 workqueue 处理完成，把 data_ready 置为 true 后，调用 wake_up_interruptible()
     *   唤醒阻塞中的读者
     */
    wait_queue_head_t read_wq;

    /*
     * 工作队列项
     *
     * write()/ioctl(SET) 不直接做慢处理，只负责登记输入并 schedule_work()。
     * 真正的处理在 worker 线程里异步执行。
     */
    struct work_struct work;

    /* 设备开关：通过 sysfs enable 控制 */
    bool enable;

    /*
     * data_ready 表示“输出结果是否已经准备好，可供 read() 读取”。
     *
     * 典型状态流转：
     *   write/ioctl(SET) -> data_ready = false
     *   work 完成       -> data_ready = true
     *   read 取走数据   -> data_ready = false
     */
    bool data_ready;

    /*
     * work_pending 表示“当前是否已经有一个异步任务在处理中”。
     *
     * 这一版为了保持实验简单，采用“单槽模型”：
     * - 同一时刻只允许一个 pending work
     * - 如果上一个 work 还没做完，新的 write/ioctl(SET) 返回 -EBUSY
     */
    bool work_pending;

    /* ioctl GET/SET 对应的整型状态值 */
    int value;

    /*
     * 输入缓冲区 / 输出缓冲区
     *
     * - input_buf：用户写入的原始内容
     * - output_buf：workqueue 异步处理后生成的结果
     */
    char input_buf[DEMO_BUF_SIZE];
    char output_buf[DEMO_BUF_SIZE];
    size_t output_len;

    /* counter 用于统计“完成过多少次异步处理” */
    unsigned int counter;

    /* log_level 通过 debugfs 导出，可动态控制日志开关 */
    unsigned int log_level;

    /* debugfs 目录节点 */
    struct dentry *debugfs_dir;
};

static struct demo_device *g_demo;

/* ============================================================
 * debugfs/status
 * ============================================================
 *
 * 这一部分用于调试观察内部状态。
 *
 * 为什么 debugfs 很重要？
 * 因为驱动很多问题不是“功能对/错”这么简单，而是：
 * - 当前是否 pending？
 * - data_ready 为什么没变？
 * - input/output 到底是什么？
 *
 * 通过 debugfs/status，就能快速看到内部快照。
 */
static int demo_status_show(struct seq_file *m, void *v)
{
    struct demo_device *dev = m->private;

    mutex_lock(&dev->lock);
    seq_printf(m, "enable=%u\n", dev->enable);
    seq_printf(m, "data_ready=%u\n", dev->data_ready);
    seq_printf(m, "work_pending=%u\n", dev->work_pending);
    seq_printf(m, "value=%d\n", dev->value);
    seq_printf(m, "counter=%u\n", dev->counter);
    seq_printf(m, "log_level=%u\n", dev->log_level);
    seq_printf(m, "input_buf=%s\n", dev->input_buf);
    seq_printf(m, "output_buf=%s\n", dev->output_buf);
    seq_printf(m, "output_len=%zu\n", dev->output_len);
    mutex_unlock(&dev->lock);

    return 0;
}

static int demo_status_open(struct inode *inode, struct file *file)
{
    return single_open(file, demo_status_show, inode->i_private);
}

static const struct file_operations demo_status_fops = {
    .owner   = THIS_MODULE,
    .open    = demo_status_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

/* ============================================================
 * sysfs: enable / counter
 * ============================================================
 *
 * sysfs 更适合做“控制面”和“状态导出”。
 *
 * - enable：控制设备开关
 * - counter：展示已经完成过多少次 work 处理
 */
static ssize_t enable_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
    struct demo_device *d = g_demo;
    bool enable;

    mutex_lock(&d->lock);
    enable = d->enable;
    mutex_unlock(&d->lock);

    return scnprintf(buf, PAGE_SIZE, "%u\n", enable ? 1 : 0);
}

static ssize_t enable_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
    struct demo_device *d = g_demo;
    unsigned long val;
    int ret;

    ret = kstrtoul(buf, 10, &val);
    if (ret)
        return ret;

    mutex_lock(&d->lock);
    d->enable = !!val;
    mutex_unlock(&d->lock);

    pr_info("demo: enable set to %u\n", !!val);
    return count;
}

static ssize_t counter_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
    struct demo_device *d = g_demo;
    unsigned int counter;

    mutex_lock(&d->lock);
    counter = d->counter;
    mutex_unlock(&d->lock);

    return scnprintf(buf, PAGE_SIZE, "%u\n", counter);
}

static DEVICE_ATTR_RW(enable);
static DEVICE_ATTR_RO(counter);

/* ============================================================
 * workqueue handler
 * ============================================================
 *
 * 这是 day05 最核心的函数之一。
 *
 * 它运行在 worker 内核线程上下文中，属于“可睡眠上下文”，因此：
 * - 可以 msleep()
 * - 可以 mutex_lock()
 *
 * 这与硬中断 top-half 有本质区别。
 */
static void demo_work_handler(struct work_struct *work)
{
    struct demo_device *dev = container_of(work, struct demo_device, work);

    if (READ_ONCE(dev->log_level))
        pr_info("demo: workqueue start\n");

    /*
     * 模拟慢处理
     *
     * 这一步是为了突出：
     * “write()/ioctl 不直接完成结果，而是异步处理后再唤醒 reader”。
     */
    msleep(500);

    mutex_lock(&dev->lock);

    /* 如果设备在处理过程中被关闭，则直接结束本次 work */
    if (!dev->enable) {
        dev->work_pending = false;
        mutex_unlock(&dev->lock);

        /*
         * 即使被关闭，也尝试唤醒等待者，避免 read 一直睡死。
         * read 醒来后会再检查 enable / data_ready 状态。
         */
        wake_up_interruptible(&dev->read_wq);
        return;
    }

    /* 生成输出结果，供后续 read() 返回给用户态 */
    scnprintf(dev->output_buf, DEMO_BUF_SIZE, "processed: %s", dev->input_buf);
    dev->output_len = strnlen(dev->output_buf, DEMO_BUF_SIZE);

    /* work 完成：设置为可读 */
    dev->data_ready = true;
    dev->work_pending = false;
    dev->counter++;

    mutex_unlock(&dev->lock);

    /* 唤醒阻塞在 read() 上的进程 */
    wake_up_interruptible(&dev->read_wq);

    if (READ_ONCE(dev->log_level))
        pr_info("demo: workqueue done, reader woken up\n");
}

/* ============================================================
 * file operations
 * ============================================================
 */
static int demo_open(struct inode *inode, struct file *file)
{
    struct demo_device *dev;

    /*
     * 从 inode->i_cdev 反推出我们的设备对象。
     * 这是字符设备里很常见的写法。
     */
    dev = container_of(inode->i_cdev, struct demo_device, cdev);
    file->private_data = dev;

    pr_info("demo: device opened\n");
    return 0;
}

static int demo_release(struct inode *inode, struct file *file)
{
    pr_info("demo: device closed\n");
    return 0;
}

/*
 * read() 运行在进程上下文。
 *
 * 这意味着：
 * - 可以睡眠
 * - 可以 wait_event_interruptible()
 * - 可以 copy_to_user()
 * - 可以使用 mutex
 *
 * 设计意图：
 * - 如果还没有结果可读，就阻塞等待
 * - 等 workqueue 完成并唤醒之后，再把 output_buf 返回给用户态
 */
static ssize_t demo_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    struct demo_device *dev = file->private_data;
    ssize_t ret;
    size_t len;

    if (!dev)
        return -ENODEV;

    /*
     * 简化实现：只支持从偏移 0 开始的一次性读取。
     * 如果 ppos != 0，直接返回 EOF。
     */
    if (*ppos != 0)
        return 0;

    /*
     * 阻塞等待条件：
     * 这里只等待 data_ready == true。
     *
     * 这样设计后，read() 的语义会更纯粹：
     * - 没有新数据时就持续阻塞
     * - 只有 workqueue 真正处理完成，并把 data_ready 置为 true 后，read() 才返回
     *
     * 这比“data_ready || !work_pending”更符合 day05 想强调的
     * “阻塞等待 + 唤醒”模型。
     *
     * 注意：如果设备在等待期间被禁用，当前 read() 仍会继续等待下一次有效数据。
     * 对 day05 学习实验来说，这样更直观。后续如果要做工程化增强，
     * 可以再引入 shutdown/error 标志来避免特殊场景下永久等待。
     */
    ret = wait_event_interruptible(dev->read_wq, dev->data_ready);
    if (ret)
        return ret;

    mutex_lock(&dev->lock);

    /* 设备被关闭时，读路径直接返回权限错误 */
    if (!dev->enable) {
        mutex_unlock(&dev->lock);
        return -EPERM;
    }

    /*
     * 醒来后仍然要二次检查。
     *
     * 因为 wake_up 只是“让你有机会醒来重新判断”，
     * 并不保证醒来时条件一定绝对稳定。
     */
    if (!dev->data_ready) {
        mutex_unlock(&dev->lock);
        return -EAGAIN;
    }

    len = min(count, dev->output_len);
    if (copy_to_user(buf, dev->output_buf, len)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    /* 数据被读走后，重新置为“无数据可读” */
    dev->data_ready = false;
    *ppos += len;

    mutex_unlock(&dev->lock);

    if (READ_ONCE(dev->log_level))
        pr_info("demo: read %zu bytes\n", len);

    return len;
}

/*
 * write() 运行在进程上下文。
 *
 * 这里故意不直接做“慢处理”，而是：
 * 1. 保存输入
 * 2. 标记 work_pending
 * 3. schedule_work()
 *
 * 这样可以清晰展示：
 * “当前调用路径只负责登记请求，真正处理在异步上下文中完成”。
 */
static ssize_t demo_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
    struct demo_device *dev = file->private_data;
    size_t len;

    if (!dev)
        return -ENODEV;

    mutex_lock(&dev->lock);

    if (!dev->enable) {
        mutex_unlock(&dev->lock);
        return -EPERM;
    }

    /* 当前已有 pending work 时，拒绝新的请求 */
    if (dev->work_pending) {
        mutex_unlock(&dev->lock);
        return -EBUSY;
    }

    len = min(count, (size_t)(DEMO_BUF_SIZE - 1));

    memset(dev->input_buf, 0, sizeof(dev->input_buf));
    memset(dev->output_buf, 0, sizeof(dev->output_buf));
    dev->output_len = 0;
    dev->data_ready = false;

    if (copy_from_user(dev->input_buf, buf, len)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    /* 手工补 '\0'，确保后续格式化输出安全 */
    dev->input_buf[len] = '\0';
    dev->work_pending = true;

    mutex_unlock(&dev->lock);

    /*
     * 提交异步工作
     *
     * 注意：这一步只是“排队”，并不代表 work 已经完成。
     */
    schedule_work(&dev->work);

    if (READ_ONCE(dev->log_level))
        pr_info("demo: write accepted, work scheduled\n");

    return count;
}

/*
 * ioctl 同样运行在进程上下文。
 *
 * GET：同步返回当前 value
 * SET：保存 value，并走和 write() 类似的异步处理路径
 */
static long demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct demo_device *dev = file->private_data;
    int val;

    if (!dev)
        return -ENODEV;

    switch (cmd) {
    case DEMO_IOCTL_SET:
        if (copy_from_user(&val, (int __user *)arg, sizeof(val)))
            return -EFAULT;

        mutex_lock(&dev->lock);

        if (!dev->enable) {
            mutex_unlock(&dev->lock);
            return -EPERM;
        }

        if (dev->work_pending) {
            mutex_unlock(&dev->lock);
            return -EBUSY;
        }

        dev->value = val;
        scnprintf(dev->input_buf, DEMO_BUF_SIZE, "ioctl-set:%d\n", val);
        memset(dev->output_buf, 0, sizeof(dev->output_buf));
        dev->output_len = 0;
        dev->data_ready = false;
        dev->work_pending = true;

        mutex_unlock(&dev->lock);

        schedule_work(&dev->work);
        return 0;

    case DEMO_IOCTL_GET:
        mutex_lock(&dev->lock);
        val = dev->value;
        mutex_unlock(&dev->lock);

        if (copy_to_user((int __user *)arg, &val, sizeof(val)))
            return -EFAULT;
        return 0;

    default:
        return -EINVAL;
    }
}

static const struct file_operations demo_fops = {
    .owner          = THIS_MODULE,
    .open           = demo_open,
    .release        = demo_release,
    .read           = demo_read,
    .write          = demo_write,
    .unlocked_ioctl = demo_ioctl,
};

/* ============================================================
 * module init / exit
 * ============================================================
 */
static int __init demo_init(void)
{
    int ret;

    g_demo = kzalloc(sizeof(*g_demo), GFP_KERNEL);
    if (!g_demo)
        return -ENOMEM;

    /* 初始化并发相关对象 */
    mutex_init(&g_demo->lock);
    init_waitqueue_head(&g_demo->read_wq);
    INIT_WORK(&g_demo->work, demo_work_handler);

    /* 初始化默认状态 */
    g_demo->enable = true;
    g_demo->data_ready = false;
    g_demo->work_pending = false;
    g_demo->value = 0;
    g_demo->counter = 0;
    g_demo->log_level = 1;

    ret = alloc_chrdev_region(&g_demo->devt, 0, 1, DEVICE_NAME);
    if (ret)
        goto err_alloc;

    cdev_init(&g_demo->cdev, &demo_fops);
    g_demo->cdev.owner = THIS_MODULE;

    ret = cdev_add(&g_demo->cdev, g_demo->devt, 1);
    if (ret)
        goto err_cdev;

    g_demo->class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(g_demo->class)) {
        ret = PTR_ERR(g_demo->class);
        goto err_class;
    }

    g_demo->device = device_create(g_demo->class, NULL, g_demo->devt, NULL,
                                   DEVICE_NAME);
    if (IS_ERR(g_demo->device)) {
        ret = PTR_ERR(g_demo->device);
        goto err_device;
    }

    ret = device_create_file(g_demo->device, &dev_attr_enable);
    if (ret)
        goto err_file1;

    ret = device_create_file(g_demo->device, &dev_attr_counter);
    if (ret)
        goto err_file2;

    g_demo->debugfs_dir = debugfs_create_dir(DEBUGFS_DIR, NULL);
    if (!g_demo->debugfs_dir) {
        ret = -ENOMEM;
        goto err_debugfs;
    }

    debugfs_create_file("status", 0444, g_demo->debugfs_dir,
                        g_demo, &demo_status_fops);
    debugfs_create_u32("log_level", 0644, g_demo->debugfs_dir,
                       &g_demo->log_level);

    pr_info("demo: Day 05 loaded. waitqueue/workqueue ready.\n");
    return 0;

err_debugfs:
    device_remove_file(g_demo->device, &dev_attr_counter);
err_file2:
    device_remove_file(g_demo->device, &dev_attr_enable);
err_file1:
    device_destroy(g_demo->class, g_demo->devt);
err_device:
    class_destroy(g_demo->class);
err_class:
    cdev_del(&g_demo->cdev);
err_cdev:
    unregister_chrdev_region(g_demo->devt, 1);
err_alloc:
    kfree(g_demo);
    return ret;
}

static void __exit demo_exit(void)
{
    if (!g_demo)
        return;

    /*
     * 非常重要：卸载前同步取消 work。
     *
     * 如果不这样做，可能出现：
     * - 模块已经卸载
     * - 内存已经释放
     * - 但 worker 线程还在访问旧指针
     *
     * 那就会触发 UAF（Use-After-Free）。
     */
    cancel_work_sync(&g_demo->work);

    debugfs_remove_recursive(g_demo->debugfs_dir);
    device_remove_file(g_demo->device, &dev_attr_counter);
    device_remove_file(g_demo->device, &dev_attr_enable);
    device_destroy(g_demo->class, g_demo->devt);
    class_destroy(g_demo->class);
    cdev_del(&g_demo->cdev);
    unregister_chrdev_region(g_demo->devt, 1);

    kfree(g_demo);

    pr_info("demo: Day 05 unloaded.\n");
}

module_init(demo_init);
module_exit(demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richer Wong");
MODULE_DESCRIPTION("Day05 demo: waitqueue + workqueue + blocking read");
