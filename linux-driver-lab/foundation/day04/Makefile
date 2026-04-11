obj-m := demo.o
# 核心路径：指向你 code 目录下的 linux-5.15.10
# 新路径：../../../kernel-src/linux-5.15.10/build/x86
# 旧绝对路径示例：/home/wq7/workspace/kernel-src/linux-5.15.10/build/x86
KDIR ?= ../../../kernel-src/linux-5.15.10/build/x86
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
