# 1. 配置内核路径
# 新路径：../../kernel-src/linux-5.15.10/build/x86
# 旧绝对路径示例：/home/wq7/workspace/kernel-src/linux-5.15.10/build/x86
KDIR ?= ../../kernel-src/linux-5.15.10/build/x86
PWD  := $(shell pwd)

# 2. 定义目标名称
MODULE_NAME := demo
APP_NAME    := test_ioctl

# 3. 内核模块编译项
obj-m := $(MODULE_NAME).o

# 4. 默认目标：同时编译驱动和测试工具
all:
	# 编译内核模块
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	# 编译用户态测试程序 (gcc test_ioctl.c -o test_ioctl)
	$(CC) --static $(APP_NAME).c -o $(APP_NAME)

# 5. 清理目标
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(APP_NAME)
