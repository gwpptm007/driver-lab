# ==============================================================================
# stage08 driver Makefile
# 编译:
#   make -C /lib/modules/$(uname -r)/build M=$PWD modules
# ==============================================================================

ifneq ($(KERNELRELEASE),)

obj-m += netdev_stage08.o

else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif
