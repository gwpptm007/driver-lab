project('media-gateway-lite', 'c', default_options: ['warning_level=2'])

dpdk = dependency('libdpdk', required: true)

executable('media-gateway-lite',
  ['main.c', 'gateway_config.c', 'gateway_rule.c', 'gateway_packet.c', 'gateway_port.c', 'gateway_stats.c'],
  dependencies: dpdk,
  install: false)
