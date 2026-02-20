project('fsv', 'c',
  version : '2.31',
  default_options : ['warning_level=2'],
  meson_version : '>= 0.55',
)

cc = meson.get_compiler('c')

# Dependencies
gtk_dep = dependency('gtk+-3.0', version : '>= 3.16')
epoxy_dep = dependency('epoxy')
m_dep = cc.find_library('m', required : true)

# Debug build option
opt_debug = get_option('fsv_debug')

# Generate config.h
conf = configuration_data()
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())
conf.set('_GNU_SOURCE', 1)
conf.set('GTK_DISABLE_COMPAT_H', 1)

# Detect 'file' command
file_cmd = find_program('file', required : false)
if file_cmd.found()
  conf.set('HAVE_FILE_COMMAND', 1)
  conf.set_quoted('FILE_COMMAND', file_cmd.full_path() + ' %s')
endif

# Check for scandir (polyfill in lib/ if missing)
have_scandir = cc.has_function('scandir', prefix : '#include <dirent.h>')
if have_scandir
  conf.set('HAVE_SCANDIR', 1)
endif

# Debug / release defines
if opt_debug
  conf.set('DEBUG', 1)
  conf.set('G_ENABLE_DEBUG', 1)
else
  conf.set('G_DISABLE_ASSERT', 1)
  conf.set('G_DISABLE_CHECKS', 1)
endif

conf.set_quoted('LOCALEDIR', 'none')

configure_file(output : 'config.h', configuration : conf)
config_inc = include_directories('.')

# Subdirectories
subdir('lib')

# Debug library (only in debug builds)
if opt_debug
  subdir('debug')
endif

# Main binary
src_sources = files(
  'src/about.c',
  'src/animation.c',
  'src/callbacks.c',
  'src/camera.c',
  'src/colexp.c',
  'src/color.c',
  'src/common.c',
  'src/dialog.c',
  'src/dirtree.c',
  'src/filelist.c',
  'src/fsv.c',
  'src/geometry.c',
  'src/gui.c',
  'src/ogl.c',
  'src/scanfs.c',
  'src/search.c',
  'src/tmaptext.c',
  'src/viewport.c',
  'src/window.c',
)

fsv_deps = [gtk_dep, epoxy_dep, m_dep]
fsv_link = [libmisc]
fsv_inc = [config_inc, include_directories('lib')]

if opt_debug
  fsv_inc += include_directories('debug')
  fsv_link += [libdebug]
endif

executable('fsv',
  src_sources,
  dependencies : fsv_deps,
  link_with : fsv_link,
  include_directories : fsv_inc,
  install : true,
)
