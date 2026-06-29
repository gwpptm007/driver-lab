project('dpdk-mbuf-inspect', 'c', default_options: ['warning_level=2'])

dpdk = dependency('libdpdk', required: true)

executable('dpdk-mbuf-inspect',
  ['main.c'],
  dependencies: dpdk,
  install: false)
