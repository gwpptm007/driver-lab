# 强制指定内核源码路径
# 当前推荐相对路径：../../kernel-src/linux-5.15.10
# 旧版本常直接写死为：/home/wq7/workspace/kernel-src/linux-5.15.10
KDIR ?= ../../kernel-src/linux-5.15.10
PWD  := $(shell pwd)

obj-m := demo.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
