# SPDX-License-Identifier: GPL-2.0
#
# driver/Makefile — 编译 netdev_stage04.ko 内核模块
#
# 用法：
#   make KDIR=/path/to/kernel/build modules
#   → 生成 netdev_stage04.ko
#
# 模块参数（insmod 时指定）：
#   ifname=xxx     net_device 名称（默认 nds4）
#   ring_size=N    TX/RX ring 深度（默认 64）
#   napi_weight=N  NAPI poll weight（默认 16）
#   rx_buf_size=N  预分配 RX buffer 大小（默认 2048）
#
# 示例：
#   insmod output/netdev_stage04.ko ifname=nds4 ring_size=8 napi_weight=2

obj-m += netdev_stage04.o
