obj-m += demo.o

# 新路径：../../../kernel-src/linux-5.15.10/build/x86
# 旧绝对路径示例：/home/wq7/workspace/kernel-src/linux-5.15.10/build/x86
KDIR ?= ../../../kernel-src/linux-5.15.10/build/x86
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(CC) -Wall -O2 -static test.c -o test

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test
