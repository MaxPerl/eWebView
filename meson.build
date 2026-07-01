project('ewebview', 'c',
  version : '0.2.1',
  license : 'LGPL-2.1',
  default_options : [
    'c_std=c11',
    'warning_level=2',
  ]
)

# ── Abhängigkeiten ─────────────────────────────────────────────────────────
webkit      = dependency('wpe-webkit-2.0')
wpe         = dependency('wpe-1.0')
wpe_fdo     = dependency('wpebackend-fdo-1.0')
elm         = dependency('elementary')
evas        = dependency('evas')
ecore       = dependency('ecore')
ecore_input = dependency('ecore-input')
glib        = dependency('glib-2.0')
xkb         = dependency('xkbcommon')
wayland     = dependency('wayland-server')

wpe_platform = dependency('wpe-platform-1.0', required: false)

# ── Shared Library ─────────────────────────────────────────────────────────
ewebview_lib = library(
  'ewebview',                          # → libewebview.so
  'ewebview.c',
  dependencies : [
    webkit, wpe, wpe_fdo, wpe_platform,
    elm, evas, ecore, ecore_input, # <-- ecore_x hier in die Linker-Liste übergeben
    glib, xkb, wayland,
  ],
  c_args : [
    # Nur Symbole mit EWEBVIEW_API werden aus der .so exportiert
    '-fvisibility=hidden',
    # Elementary ist verfügbar (für Scroll-Accel-Faktor etc.)
    '-DHAS_ELEMENTARY=1',
  ],
  # soname: libewebview.so.0 → libewebview.so.0.1.0
  version  : meson.project_version(),
  soversion : '0',
  install  : true,
)

# ── Header installieren ────────────────────────────────────────────────────
install_headers('ewebview.h')

# ── pkg-config generieren ──────────────────────────────────────────────────
pkg = import('pkgconfig')
pkg.generate(
  ewebview_lib,
  name        : 'ewebview',
  description : 'WPEWebKit Evas Smart Object — WebKit in EFL',
  version     : meson.project_version(),
  filebase    : 'ewebview',
  # Was Nutzer der Library zusätzlich linken müssen:
  requires    : ['elementary', 'evas'],
)

# ── Demo-Browser ───────────────────────────────────────────────────────────
executable(
  'ewebview-browser',
  'browser.c',
  dependencies : [elm, evas, ecore],
  link_with    : ewebview_lib,
  c_args       : ['-DHAS_ELEMENTARY=1'],
  install      : true,
)
