# day22/driver/Makefile
#
# 这是 day22 自己的内核模块构建入口。
# 不去依赖前面任何 day 的模块目录。
#
# 用法示例：
#   make -C driver KDIR=/path/to/linux/build
#
# 或者在 day22 目录下：
#   make module KDIR=/path/to/linux/build

ifneq ($(KERNELRELEASE),)
obj-m += day22_ivshmem_stub.o
else
KDIR ?=
PWD  := $(shell pwd)

all:
	@if [ -z "$(KDIR)" ]; then \
		echo "[day22-driver] 请指定 KDIR=/path/to/kernel/build"; \
		exit 1; \
	fi
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	@if [ -n "$(KDIR)" ]; then \
		$(MAKE) -C $(KDIR) M=$(PWD) clean; \
	else \
		rm -rf *.o *.ko *.mod *.mod.c *.symvers *.order .*.cmd .tmp_versions Module.symvers modules.order; \
	fi
endif
