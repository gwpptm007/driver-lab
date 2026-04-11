obj-m += demo_of.o

# 推荐先导出：
#   export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
#   export CROSS_COMPILE=aarch64-linux-gnu-
# 然后直接执行：
#   make
#   make clean
KERNEL_DIR ?= ../../../kernel-src/linux-5.15.10
KDIR ?= $(KERNEL_DIR)/build/arm64
PWD  := $(shell pwd)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f rootfs.img virt-base.dtb virt-base.dts virt-day09.dts virt-day09.dtb
	rm -rf rootfs
