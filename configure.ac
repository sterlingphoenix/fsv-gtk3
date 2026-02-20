# Process this file with autoconf to produce a configure script.
AC_INIT([fsv], [2.30])
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

# GTK+ 3.0 libraries (3.16+ required for GtkGLArea)
PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= 3.16])
PKG_CHECK_MODULES([GLIB], [glib-2.0])

# libepoxy for OpenGL function loading (replaces manual GL/GLX)
PKG_CHECK_MODULES([EPOXY], [epoxy])

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
	lib/Makefile
	src/Makefile
])
AC_OUTPUT
