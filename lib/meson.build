lib_sources = files('getopt.c', 'getopt1.c', 'nvstore.c')

if not have_scandir
  lib_sources += files('scandir.c')
endif

libmisc = static_library('misc',
  lib_sources,
  dependencies : [gtk_dep],
  include_directories : [config_inc],
)
