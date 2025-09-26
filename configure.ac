#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.72])
AC_INIT([letters],
	m4_esyscmd([{ git describe --tags --dirty || echo unknown; } | tr -d \\n]),
	[william.r.pursell@gmail.com])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([letters.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([curses], [getch])
AC_CHECK_LIB([termcap], [tgetent])

# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strtol])

eval eval eval abs_datadir="$datadir"
AC_DEFINE_UNQUOTED([DATADIR], ["$abs_datadir"], [Absolute path to data dir])
AC_SUBST([DATADIR], [$abs_datadir])

AC_CONFIG_FILES([Makefile letters.man])
AC_OUTPUT
