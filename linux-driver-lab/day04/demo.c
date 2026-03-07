#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/debugfs.h>

#define DEMO_NAME "demo_day04"
#define DEMO_IOCTL_MAGIC 'k'
#define DEMO_IOCTL_GET _IOR(DEMO_IOCTL_MAGIC, 1, int)
#define DEMO_IOCTL_SET _IOW(DEMO_IOCTL_MAGIC, 2, int)

struct demo_device {
    struct cdev cdev;
    struct class *class;
    struct device *device;
    dev_t dev_id;
    
    int enable;         /* Day 03 开关 */
    int counter;        /* Day 03 计数器 */
    u32 log_level;      /* Day 04 日志开关 */
    struct dentry *debug_root; /* Day 04 Debugfs 句柄 */
};

static struct demo_device *g_demo_dev;

/* --- Day 04: Debugfs 状态快照读取 --- */
static ssize_t demo_status_read(struct file *file, char __user *user_buf, 
                                size_t count, loff_t *ppos)
{
    char buf[512];
    int len;

    len = snprintf(buf, sizeof(buf),
                   "---------- Day 04 Snapshot ----------\n"
                   "Device Enable : %s\n"
                   "IOCTL Count   : %d\n"
                   "Debug Log Lvl : %u\n"
                   "Kernel Time   : %lu s\n"
                   "-------------------------------------\n",
                   g_demo_dev->enable ? "ON" : "OFF",
                   g_demo_dev->counter,
                   g_demo_dev->log_level,
                   (unsigned long)(jiffies / HZ));

    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations debug_status_fops = {
    .owner = THIS_MODULE,
    .read  = demo_status_read,
};

/* --- Day 03: Sysfs 接口 --- */
static ssize_t enable_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", g_demo_dev->enable);
}
static ssize_t enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    if (kstrtoint(buf, 10, &g_demo_dev->enable) < 0) return -EINVAL;
    return count;
}
static DEVICE_ATTR_RW(enable);

/* --- Day 02: IOCTL 实现 --- */
static long demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (!g_demo_dev->enable) {
        if (g_demo_dev->log_level > 0)
            pr_info_ratelimited("Demo: Device disabled, IOCTL rejected!\n");
        return -EPERM;
    }

    switch (cmd) {
        case DEMO_IOCTL_GET:
        case DEMO_IOCTL_SET:
            g_demo_dev->counter++;
            break;
        default:
            return -EINVAL;
    }
    
    if (g_demo_dev->log_level > 0)
        pr_info_ratelimited("Demo: IOCTL handled (Count: %d)\n", g_demo_dev->counter);
        
    return 0;
}

static struct file_operations demo_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = demo_ioctl,
};

static int __init demo_init(void)
{
    g_demo_dev = kzalloc(sizeof(struct demo_device), GFP_KERNEL);
    if (!g_demo_dev) return -ENOMEM;

    alloc_chrdev_region(&g_demo_dev->dev_id, 0, 1, DEMO_NAME);
    cdev_init(&g_demo_dev->cdev, &demo_fops);
    cdev_add(&g_demo_dev->cdev, g_demo_dev->dev_id, 1);
    
    g_demo_dev->class = class_create(THIS_MODULE, DEMO_NAME);
    g_demo_dev->device = device_create(g_demo_dev->class, NULL, g_demo_dev->dev_id, NULL, DEMO_NAME);
    
    device_create_file(g_demo_dev->device, &dev_attr_enable);
    g_demo_dev->enable = 1;

    /* Day 04 Debugfs 注册 */
    g_demo_dev->debug_root = debugfs_create_dir("demo_debug", NULL);
    if (g_demo_dev->debug_root) {
        debugfs_create_file("status", 0444, g_demo_dev->debug_root, NULL, &debug_status_fops);
        debugfs_create_u32("log_level", 0644, g_demo_dev->debug_root, &g_demo_dev->log_level);
    }

    pr_info("Demo Day 04: Initialized with debugfs\n");
    return 0;
}

static void __exit demo_exit(void)
{
    debugfs_remove_recursive(g_demo_dev->debug_root);
    device_destroy(g_demo_dev->class, g_demo_dev->dev_id);
    class_destroy(g_demo_dev->class);
    cdev_del(&g_demo_dev->cdev);
    unregister_chrdev_region(g_demo_dev->dev_id, 1);
    kfree(g_demo_dev);
    pr_info("Demo Day 04: Removed\n");
}

module_init(demo_init);
module_exit(demo_exit);
MODULE_LICENSE("GPL");
