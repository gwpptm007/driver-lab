#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h> // 包含 copy_to_user 和 copy_from_user
#include "demo_ioctl.h"    // 引入自定义的 ioctl 命令号

/* 定义一个内核全局变量，用于模拟设备内部的状态或寄存器 */
static int kernel_value = 0;

/* 1. 实现 Open 回调 */
static int demo_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Demo: Device opened\n");
    return 0;
}

/* 2. 实现 Release 回调 */
static int demo_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Demo: Device closed\n");
    return 0;
}

/* 3. 实现 unlocked_ioctl 回调 (Day 02 核心) */
/* file: 文件指针, cmd: 命令号, arg: 用户传来的参数(可能是数值或指针地址) */
static long demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    /* 强制转换为用户态 int 指针，方便后续搬运 */
    int __user *user_ptr = (int __user *)arg;
    int temp;

    switch (cmd) {
    case DEMO_SET_VAL:
        /* 安全地将用户态数据拷贝到内核 temp 变量 */
        /* 返回值非 0 表示拷贝失败（通常是无效的用户态地址） */
        if (copy_from_user(&temp, user_ptr, sizeof(int))) {
            return -EFAULT; // 返回错误码：坏地址
        }
        kernel_value = temp;
        printk(KERN_INFO "Demo: Set value to %d\n", kernel_value);
        break;

    case DEMO_GET_VAL:
        /* 安全地将内核变量 kernel_value 拷贝到用户态地址 */
        if (copy_to_user(user_ptr, &kernel_value, sizeof(int))) {
            return -EFAULT;
        }
        printk(KERN_INFO "Demo: Get value %d\n", kernel_value);
        break;

    default:
        /* 如果用户传了不匹配的命令号，返回无效参数 */
        printk(KERN_WARNING "Demo: Unknown ioctl command 0x%x\n", cmd);
        return -EINVAL; // 返回错误码：无效参数
    }

    return 0; // 执行成功返回 0
}

/* 4. 填充文件操作结构体 */
static struct file_operations demo_fops = {
    .owner          = THIS_MODULE,
    .open           = demo_open,
    .release        = demo_release,
    .unlocked_ioctl = demo_ioctl, // 注册 ioctl 处理函数
};

/* 5. 填充杂项设备结构体 */
static struct miscdevice demo_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "demo",
    .fops  = &demo_fops,
};

/* 6. 标准的加载/卸载函数 */
static int __init demo_init(void) {
    int ret = misc_register(&demo_misc);
    if (ret) {
        printk(KERN_ERR "Demo: Failed to register misc device\n");
        return ret;
    }
    printk(KERN_INFO "Demo: Module loaded with IOCTL support\n");
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
