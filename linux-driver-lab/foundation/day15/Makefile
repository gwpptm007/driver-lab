# Day15 Makefile
# --------------
# Day15 现在按“自包含目录”组织，所以这里不再借用 day13/ 的模块编译入口，
# 而是由 day15 自己维护 demo_regmap.ko 的编译。
#
# 这里仍然使用 demo_regmap.c，有两个考虑：
# 1. 它已经具备 debugfs + trigger + snapshot 这些 Day15 baseline 采样需要的接口；
# 2. W3 这一周的重点是内核裁剪与系统观测，不是再发明新的驱动功能点。
#
# 也就是说：
# - Day15 复用的是“教学驱动能力”
# - 但构建、rootfs、DTB、采样链路，都在 day15/ 自己目录里完成

obj-m += demo_regmap.o

KDIR ?= ../../../kernel-src/linux-5.15.10/build/arm64
PWD  := $(shell pwd)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
