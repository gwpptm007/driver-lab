# Minimal placeholder. Adapt to your DPDK installation.
project('mini_l2fwd', 'c')

dpdk = dependency('libdpdk', required: true)
executable('mini_l2fwd', 'main.c', dependencies: dpdk)
