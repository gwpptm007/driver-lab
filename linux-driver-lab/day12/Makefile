# Day12 在 day11 的 platform + IRQ + workqueue 骨架上继续前进：
# 这次把运行态统计整理成一组 shadow registers，并通过 regmap 暴露统一访问入口。
# 模块名改为 demo_regmap，方便和前面的 day10 / day11 区分。

obj-m += demo_regmap.o

KDIR ?= ../../kernel-src/linux-5.15.10/build/arm64
PWD  := $(shell pwd)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
