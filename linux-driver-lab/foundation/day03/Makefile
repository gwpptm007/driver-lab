# 新路径：../../../kernel-src/linux-5.15.10/build/x86
# 旧绝对路径示例：/home/wq7/workspace/kernel-src/linux-5.15.10/build/x86
KDIR ?= ../../../kernel-src/linux-5.15.10/build/x86
PWD  := $(shell pwd)
obj-m := demo.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	# 显式包含当前目录 (-I.) 编译当前目录下的测试程序
	$(CC) -static -I. test_ioctl.c -o test_ioctl

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test_ioctl
