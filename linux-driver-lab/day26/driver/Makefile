# day26 外部模块 Makefile
#
# 关键点：这里不能使用 
#   M=$(PWD)
# 因为顶层脚本会用
#   make -C ${DAY26_ROOT}/driver ...
# 进入本目录。此时 shell 环境里的 PWD 仍可能保留为外层 day26 目录，
# 会导致 Kbuild 把模块产物输出到错误位置（例如 day26/ 顶层），
# 进而出现：日志说“模块已生成”，但 driver/day26_edu_tool.ko 实际不存在。
#
# GNU make 在处理 -C 之后会把 CURDIR 更新为“当前真正的工作目录”，
# 因此这里必须使用 $(CURDIR)，确保：
#   M=<day26>/driver
# 这样 .o/.ko/Module.symvers 都会稳定生成在 day26/driver/ 下。

obj-m += day26_edu_tool.o

KDIR ?= /lib/modules/$(shell uname -r)/build
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
