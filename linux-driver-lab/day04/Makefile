obj-m := demo.o
# 核心路径：指向你 code 目录下的 linux-5.15.10
# 当前推荐相对路径：../../kernel-src/linux-5.15.10
# 旧版本常直接写死为：/home/wq7/workspace/kernel-src/linux-5.15.10
KDIR ?= ../../kernel-src/linux-5.15.10
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
