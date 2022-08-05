x11_use = true
x11_deps = [
  dependency('x11', required: get_option('x11')),
  dependency('xi', required: get_option('x11')),
  dependency('glx', version: '>= 1.3', required: get_option('x11'))
]

foreach dep: x11_deps
  if not dep.found()
    x11_use = false
    break
  endif
endforeach

if x11_use
  client_src += files('x11.c')
  client_deps += x11_deps
  config.set10('USE_X11', true)
endif

wayland_opt = get_option('wayland')
wayland_deps = [
  dependency('wayland-client', version: '>= 1.15.0', required: wayland_opt),
  dependency('wayland-cursor', version: '>= 1.15.0', required: wayland_opt),
  dependency('wayland-egl', version: '>= 9.0.0', required: wayland_opt),
  dependency('wayland-protocols', version: '>= 1.15', required: wayland_opt),
  dependency('egl', version: '>= 1.5', required: wayland_opt)
]

wayland_protocols = [
  'stable/xdg-shell/xdg-shell.xml',
  'unstable/pointer-constraints/pointer-constraints-unstable-v1.xml',
  'unstable/primary-selection/primary-selection-unstable-v1.xml',
  'unstable/relative-pointer/relative-pointer-unstable-v1.xml',
  'unstable/xdg-decoration/xdg-decoration-unstable-v1.xml'
]

foreach dep: wayland_deps
  if not dep.found()
    subdir_done()
  endif
endforeach

if not cc.has_header('linux/input-event-codes.h', required: wayland_opt)
  subdir_done()
endif

wl_scanner = find_program('wayland-scanner', required: wayland_opt)
if not wl_scanner.found()
  subdir_done()
endif

wl_protocols_dir = wayland_deps[3].get_variable('pkgdatadir')
wl_protocols_source = []
wl_protocols_headers = []

foreach p: wayland_protocols
  xml = wl_protocols_dir / p
  wl_protocols_source += custom_target(xml.underscorify() + '_c',
    input: xml,
    output: '@BASENAME@.c',
    command: [wl_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
  wl_protocols_headers += custom_target(xml.underscorify() + '_h',
    input: xml,
    output: '@BASENAME@.h',
    command: [wl_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
endforeach

wl_protocols_lib = static_library('wl_protocols', wl_protocols_source + wl_protocols_headers)
wl_protocols_dep = declare_dependency(link_with: wl_protocols_lib, sources: wl_protocols_headers)

client_src += files('wayland.c')
client_deps += [wayland_deps, wl_protocols_dep]
config.set10('USE_WAYLAND', true)