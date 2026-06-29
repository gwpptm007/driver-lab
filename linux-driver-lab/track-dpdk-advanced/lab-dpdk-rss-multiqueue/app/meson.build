project('dpdk-rss-queue-probe', 'c', default_options: ['warning_level=2'])

dpdk = dependency('libdpdk', required: true)

executable('dpdk-rss-queue-probe',
  ['main.c'],
  dependencies: dpdk,
  install: false)
