# Process this file with autoconf to produce a configure script.
AC_INIT([fsv], [1.0])
AC_CONFIG_SRCDIR([src/fsv.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign -Wall])

AC_DEFINE([_GNU_SOURCE], [1], [Enable GNU extensions])
AC_DEFINE([GTK_DISABLE_COMPAT_H], [1], [Disable GTK 1.x compat])

# Programs
AC_PROG_CC
AC_PROG_CPP
AC_PROG_RANLIB

# Header files
AC_HEADER_DIRENT
AC_HEADER_TIME
AC_CHECK_HEADERS([strings.h sys/time.h unistd.h])

# Typedefs, structures
AC_C_CONST
AC_TYPE_MODE_T
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_TYPE([comparison_fn_t], [],
  [AC_DEFINE_UNQUOTED([comparison_fn_t],
    [int (*)(const void *, const void *)],
    [Define comparison_fn_t if not available])])
AC_STRUCT_ST_BLOCKS
AC_STRUCT_TM

# Library functions
AC_FUNC_ALLOCA
AC_CHECK_FUNCS([getcwd gettimeofday mktime strcspn strdup strspn strtod strtoul fnmatch strftime])
AC_REPLACE_FUNCS([scandir])
AC_CHECK_LIB([m], [hypot])

# Debugging
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [turn on debugging])])
if test "$enable_debug" = "yes" ; then
	AC_MSG_WARN([debugging enabled, CFLAGS="$CFLAGS"])
	AM_CONDITIONAL([DEBUG], [true])
	AC_DEFINE([DEBUG], [1], [Enable debugging])
	AC_DEFINE([G_ENABLE_DEBUG], [1], [Enable GLib debugging])
else
	AM_CONDITIONAL([DEBUG], [false])
	AC_DEFINE([G_DISABLE_ASSERT], [1], [Disable GLib assertions])
	AC_DEFINE([G_DISABLE_CHECKS], [1], [Disable GLib checks])
fi

# Locale directory
localedir="none"
AC_SUBST([localedir])

# Documentation directory
AC_ARG_WITH([doc-dir],
  [AS_HELP_STRING([--with-doc-dir=DIR], [install documentation files in DIR [DATADIR/fsv]])])
if test -n "$with_doc_dir" ; then
	docdir=$with_doc_dir
else
	docdir=${datadir}/fsv
fi
AC_SUBST([docdir])

# GTK+ 2.0 libraries
PKG_CHECK_MODULES([GTK], [gtk+-2.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0])

#### OpenGL/Mesa3D libraries ####
AC_ARG_WITH([GL-prefix],
  [AS_HELP_STRING([--with-GL-prefix=PFX], [Prefix where GL is installed (optional)])])

if test -n "$with_GL_prefix" ; then
	GL_CFLAGS=-I$with_GL_prefix/include
	GL_LDOPTS=-L$with_GL_prefix/lib
else
	GL_CFLAGS=""
	GL_LDOPTS=""
fi

AC_CHECK_HEADER([GL/glu.h], [have_GLU="yes"], [have_GLU="no"])
AC_CHECK_LIB([GL], [glBegin], [have_GL=yes], [], [$GTK_LIBS $GL_LDOPTS])

if test "$have_GLU" = "no" ; then
	AC_MSG_ERROR([Missing OpenGL utility library (GLU)])
fi

if test "$have_GL" = "yes" ; then
	GL_LIBS="$GL_LDOPTS -lGL -lGLU"
else
	AC_MSG_ERROR([You need GL libraries])
fi
AC_SUBST([GL_CFLAGS])
AC_SUBST([GL_LIBS])
#### end of OpenGL/Mesa3D configuration ####

# Check for X11 (needed for GLX)
PKG_CHECK_MODULES([X11], [x11])

# Check for 'file' command
AC_PATH_PROG([FILE_CMD], [file])
if test -n "$FILE_CMD" ; then
	AC_DEFINE([HAVE_FILE_COMMAND], [1], [Define if file command is available])
	AC_DEFINE_UNQUOTED([FILE_COMMAND], ["$FILE_CMD %s"], [file command template])
fi

# That's a wrap!
AC_CONFIG_FILES([
	Makefile
	debug/Makefile
	doc/Makefile
	lib/Makefile
	src/Makefile
])
AC_OUTPUT
