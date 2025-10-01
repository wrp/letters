#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([letters],
	m4_esyscmd([{ git describe --tags --dirty || echo unknown; } | tr -d \\n]),
	[william.r.pursell@gmail.com])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([letters.c])
AC_CONFIG_HEADERS([config.h])

AC_ARG_VAR([DICTIONARY], [Define word dir (default: /usr/share/dict/words)])
: "${DICTIONARY:=/usr/share/dict/words}"
AS_IF([! test -f "$DICTIONARY"],
	[AC_MSG_ERROR([word list file $DICTIONARY does not exit])])

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
AC_SUBST([DATADIR], [$abs_datadir])

AC_SUBST([DICTIONARY])

AC_DEFINE_UNQUOTED([HIGHSCORES], ["$abs_datadir/letters.high"],
	[Path to high score file])

AC_CONFIG_FILES([Makefile letters.man])
AC_OUTPUT
