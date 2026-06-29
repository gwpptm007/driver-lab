project('dpdk-l3-forwarder-lite', 'c', default_options: ['warning_level=2'])
dpdk = dependency('libdpdk', required: true)
executable('dpdk-l3-forwarder-lite', ['main.c'], dependencies: dpdk, install: false)

