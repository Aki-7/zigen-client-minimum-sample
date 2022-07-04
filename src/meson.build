source_files = [
  'app.c',
  'box.c',
  'main.c',
  zigen_protocol_c,
  zigen_client_protocol_h,
  zigen_shell_protocol_c,
  zigen_shell_client_protocol_h,
  zigen_opengl_protocol_c,
  zigen_opengl_client_protocol_h,
]


dependencies = [
  dep_wayland_client
]

executable(
  'client',
  source_files,
  install : false,
  dependencies : dependencies,
)
