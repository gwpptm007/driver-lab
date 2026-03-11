# day10 只需要编译一个外部模块：demo_irq.ko
obj-m += demo_irq.o

# 这里默认指向当前仓库里的 5.15.10 工作区。
# 真正编译时，build.sh 会把 KDIR/ARCH/CROSS_COMPILE 显式传进来。
KERNEL_DIR ?= ../../kernel-src/linux-5.15.10
KDIR ?= $(KERNEL_DIR)/build/arm64
PWD  := $(shell pwd)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

# 外部模块标准编译入口：
# make -C <kernel-build-dir> M=<module-dir> modules
all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

# 清理模块构建产物，同时把 day10 运行中间产物也清掉。
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f rootfs.img virt-base.dtb virt-base.dts virt-irq.dts virt-irq.dtb
	rm -rf rootfs
