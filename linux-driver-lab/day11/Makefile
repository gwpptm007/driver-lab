# Day11 继续沿用 day09/day10 的 out-of-tree 模块写法
# 这里把模块名换成 demo_irq_wq，表示它已经引入了 workqueue bottom-half

obj-m += demo_irq_wq.o

KDIR ?= ../../kernel-src/linux-5.15.10/build/arm64
PWD  := $(shell pwd)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
