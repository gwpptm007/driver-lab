obj-m += day23_ivshmem_probe.o

KDIR ?=
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

all:
	@if [ -z "$(KDIR)" ]; then echo "[day23][ERROR] KDIR 未设置"; exit 1; fi
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	@if [ -n "$(KDIR)" ]; then 		$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean; 	else 		rm -f *.o *.ko *.mod *.mod.c *.symvers *.order Module.symvers modules.order; 	fi
