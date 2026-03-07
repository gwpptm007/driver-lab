obj-m += demo.o

# 当前推荐相对路径：../../kernel-src/linux-5.15.10
# 旧版本常直接写死为：/home/wq7/workspace/kernel-src/linux-5.15.10
KDIR ?= ../../kernel-src/linux-5.15.10
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(CC) -Wall -O2 -static test.c -o test

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test
