# Day17 Makefile
# --------------
# Day17 不再依赖 day15/day16 的构建入口，而是把“模块编译”这一步直接收口到
# 自己目录里。这样你在 day17/ 内执行 make 时，就能清晰知道：
#
# 1. 当前编译的教学模块是什么      -> demo_regmap.ko
# 2. 当前使用的内核输出目录是什么   -> KDIR
# 3. 当前目标架构和交叉编译器是什么 -> ARCH / CROSS_COMPILE
#
# 这里仍然复用 demo_regmap.c，而不是再造一个新驱动，原因是：
# - 它已经把 DT / IRQ / workqueue / regmap / debugfs / function_graph 这些
#   W1~W2 的关键知识点串起来了；
# - Day17 的重点是“独立实验目录 + 构建链 + 裁剪链 + 采样链”，不是重新发明
#   一个功能点；
# - 继续使用同一个教学驱动，反而更利于你把注意力放到内核配置、rootfs、QEMU、
#   tracing、perf、结果对比这些 W3 真正的主线上。

obj-m += demo_regmap.o

KDIR ?= ../../kernel-src/linux-5.15.10/build/arm64
PWD  := $(shell pwd)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
