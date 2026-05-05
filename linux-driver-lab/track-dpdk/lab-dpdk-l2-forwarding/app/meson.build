project('l2fwd-lite', 'c',
  version: '0.1.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
  ]
)

dpdk = dependency('libdpdk', required: true)

executable('l2fwd-lite',
  'main.c',
  dependencies: dpdk,
  install: false,
)
