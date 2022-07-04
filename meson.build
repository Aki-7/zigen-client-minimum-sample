project(
  'zigen client minimum sample',
  'c',
  version : '0.0.1',
  default_options : ['warning_level=3'],
  meson_version : '>=0.58.0',
)

dep_wayland_scanner = dependency('wayland-scanner')
dep_wayland_client = dependency('wayland-client')
dep_zgn_protocol = dependency('zigen-protocols')

subdir('protocol')
subdir('src')
