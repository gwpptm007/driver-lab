project('fastpath-lite', 'c', default_options: ['warning_level=2'])

dpdk_dep = dependency('libdpdk')

executable('fastpath-lite',
  'main.c',
  dependencies: dpdk_dep,
  install: false)
