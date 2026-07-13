# Meson 入口与 Makefile 使用同一组源文件，便于接入标准 DPDK 构建环境。
project('dpdk-flow-pipeline', 'c', default_options: ['c_std=gnu11', 'warning_level=3'])
dpdk = dependency('libdpdk', required: true)
sources = ['main.c', 'flow_key.c', 'flow_table.c', 'flow_pipeline.c', 'flow_capability.c', 'flow_worker.c']
executable('dpdk-flow-pipeline', sources, dependencies: dpdk, install: false)
