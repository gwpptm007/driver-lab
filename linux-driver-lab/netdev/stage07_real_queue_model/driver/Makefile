# ================================================================================
# stage07 driver/Makefile — kernel 模块编译规则
#
# 【用法】
#   make KDIR=/path/to/kernel/build     # 编译模块
#   make KDIR=/path/to/kernel/build clean  # 清理
#
# 【KDIR 参数】
# 指向 kernel build 目录（必须包含 Module.symvers / .config 等）
# - 本机默认: /lib/modules/$(uname -r)/build
# - ARM64:    /path/to/arm64/kernel/build
#
# 【编译原理】
# 外部模块编译使用 kernel make system:
#   make -C KDIR M=PWD modules
#   -C KDIR : 切换到 kernel build 目录
#   M=PWD   : 告诉 kernel make 当前目录是外部模块
#   modules : kernel build target 生成 .ko 文件
#
# 【输出】
# netdev_stage07.ko — 可加载的 ELF kernel module
# ================================================================================

# obj-m: 告诉 make 这是一个外部 kernel 模块（不是 built-in）
obj-m += netdev_stage07.o

# KDIR: kernel build directory（必须通过命令行传入）
# ?= 表示默认值（只在未定义时生效）
KDIR ?= /lib/modules/$(shell uname -r)/build

# PWD: 当前目录（模块源码目录）
PWD  := $(shell pwd)

# all: 默认目标 — 编译模块
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# clean: 清理编译产物
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
