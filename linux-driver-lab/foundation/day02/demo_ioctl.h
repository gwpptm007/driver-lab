#ifndef _DEMO_IOCTL_H_
#define _DEMO_IOCTL_H_

#include <linux/ioctl.h> // 提供构造命令号的宏

// 定义幻数（Magic Number），通常选择一个没被占用的字符
#define DEMO_MAGIC 'd'

// 使用宏定义命令号
// _IOW: 写数据到内核 (SET)
// _IOR: 从内核读数据 (GET)
#define DEMO_SET_VAL _IOW(DEMO_MAGIC, 0, int)
#define DEMO_GET_VAL _IOR(DEMO_MAGIC, 1, int)

#endif
