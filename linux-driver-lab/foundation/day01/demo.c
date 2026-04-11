#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

// 1. 实现 Open 回调
static int demo_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Demo: Device opened\n");
    return 0;
}

// 2. 实现 Release 回调
static int demo_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Demo: Device closed\n");
    return 0;
}

// 3. 实现 Write 回调
static ssize_t demo_write(struct file *f, const char __user *buf, size_t count, loff_t *pos) {
    printk(KERN_INFO "Demo: Received %zu bytes of data\n", count);
    return count; 
}

// 4. 填充文件操作结构体
static struct file_operations demo_fops = {
    .owner   = THIS_MODULE,
    .open    = demo_open,
    .release = demo_release,
    .write   = demo_write,
};

// 5. 填充杂项设备结构体
static struct miscdevice demo_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "demo",
    .fops  = &demo_fops,
};

// 6. 标准的加载/卸载函数
static int __init demo_init(void) {
    int ret = misc_register(&demo_misc);
    if (ret) {
        printk(KERN_ERR "Demo: Failed to register misc device\n");
        return ret;
    }
    printk(KERN_INFO "Demo: Module loaded, device /dev/demo created\n");
    return 0;
}

static void __exit demo_exit(void) {
    misc_deregister(&demo_misc);
    printk(KERN_INFO "Demo: Module unloaded\n");
}

module_init(demo_init);
module_exit(demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richer Wong");
