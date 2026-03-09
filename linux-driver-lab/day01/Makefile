# 强制指定内核源码路径
# 新路径：../../kernel-src/linux-5.15.10/build/x86
# 旧绝对路径示例：/home/wq7/workspace/kernel-src/linux-5.15.10/build/x86
KDIR ?= ../../kernel-src/linux-5.15.10/build/x86
PWD  := $(shell pwd)

obj-m := demo.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
