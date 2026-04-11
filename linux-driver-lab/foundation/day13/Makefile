# Day13 Makefile 说明
# -------------------
# 这里仍然只编译一个外部模块：demo_regmap.ko。
# 原因不是懒得改名字，而是有意保持和 day12 一致：
#
# 1. trigger / snapshot / poke 接口保持连续
# 2. function_graph 追踪的是同一套教学驱动
# 3. 避免为了看 trace 再换驱动名，增加理解成本
#
# obj-m += demo_regmap.o 表示：
# 把 demo_regmap.c 编成一个可加载内核模块 demo_regmap.ko。
#
# KDIR/ARCH/CROSS_COMPILE 这三个变量来自外部模块编译的经典写法：
# - KDIR          : 目标内核的构建目录
# - ARCH          : 目标架构，这里默认 arm64
# - CROSS_COMPILE : 交叉编译器前缀

obj-m += demo_regmap.o

KDIR ?= ../../../kernel-src/linux-5.15.10/build/arm64
PWD  := $(shell pwd)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
