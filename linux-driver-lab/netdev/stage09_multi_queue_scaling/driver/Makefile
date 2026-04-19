ifneq ($(KDIR),)
KERNEL_DIR := $(KDIR)
else
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
endif

obj-m += netdev_stage09.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) clean
