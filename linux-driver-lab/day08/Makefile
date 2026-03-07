obj-m += demo_pdrv.o

# 当前推荐相对路径是 ../../kernel-src/linux-5.15.10
# 旧版本也兼容 /home/wq7/workspace/kernel-src/linux-5.15.10
KDIR ?= ../../kernel-src/linux-5.15.10
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f rootfs.img
	rm -rf rootfs
