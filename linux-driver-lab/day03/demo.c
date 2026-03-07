#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "demo_ioctl.h"

static int major;
static int demo_enable = 1;   // 开关变量
static int demo_counter = 0;  // 计数器变量

static struct class *demo_class;
static struct device *demo_device;

// --- 1. Sysfs 读写回调函数 ---

static ssize_t enable_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", demo_enable);
}

static ssize_t enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    int val;
    if (kstrtoint(buf, 10, &val) == 0) {
        demo_enable = (val > 0) ? 1 : 0;
        pr_info("Demo: Enable set to %d\n", demo_enable);
    }
    return count;
}

static ssize_t counter_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", demo_counter);
}

// --- 2. 声明属性文件 ---
static DEVICE_ATTR_RW(enable);   // 可读写属性
static DEVICE_ATTR_RO(counter);  // 只读属性

// --- 3. 字符设备操作 ---

static long demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    int val = 88;

    // 打印收到的命令号，方便对比
    pr_info("Demo: Received cmd=0x%x, expected GET=0x%lx, SET=0x%lx\n", 
             cmd, (unsigned long)DEMO_IOCTL_GET, (unsigned long)DEMO_IOCTL_SET);
    
    // Day 03 核心逻辑：检查开关
    if (!demo_enable) {
        pr_warn("Demo: Device is disabled! Operation rejected.\n");
        return -EPERM; // Operation not permitted
    }

    switch(cmd) {
        case DEMO_IOCTL_GET:
            if (copy_to_user((int __user *)arg, &val, sizeof(int))) return -EFAULT;
            demo_counter++; // 成功操作，计数加1
            break;
        case DEMO_IOCTL_SET:
            if (copy_from_user(&val, (int __user *)arg, sizeof(int))) return -EFAULT;
            demo_counter++; // 成功操作，计数加1
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

static const struct file_operations demo_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = demo_ioctl,
    .open = (void*)0, // 简单起见，暂不处理 open
    .release = (void*)0,
};

// --- 4. 驱动加载与卸载 ---

static int __init demo_init(void) {
    major = register_chrdev(0, "demo_device", &demo_fops);
    
    // 创建 /sys/class/demo/ 目录
    demo_class = class_create(THIS_MODULE, "demo");
    
    // 创建 /sys/class/demo/demo0/ 目录并自动创建 /dev/demo0
    demo_device = device_create(demo_class, NULL, MKDEV(major, 0), NULL, "demo");

    // 在 /sys/class/demo/demo0/ 下创建属性文件
    device_create_file(demo_device, &dev_attr_enable);
    device_create_file(demo_device, &dev_attr_counter);

    pr_info("Demo: Day 03 loaded. Sysfs ready.\n");
    return 0;
}

static void __exit demo_exit(void) {
    device_remove_file(demo_device, &dev_attr_counter);
    device_remove_file(demo_device, &dev_attr_enable);
    device_destroy(demo_class, MKDEV(major, 0));
    class_destroy(demo_class);
    unregister_chrdev(major, "demo_device");
    pr_info("Demo: Day 03 unloaded.\n");
}

module_init(demo_init);
module_exit(demo_exit);
MODULE_LICENSE("GPL");
