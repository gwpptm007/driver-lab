project('dpdk-burst-cache-probe', 'c', default_options: ['warning_level=2'])
dpdk = dependency('libdpdk', required: true)
executable('dpdk-burst-cache-probe', ['main.c'], dependencies: dpdk, install: false)
