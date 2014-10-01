AC_PREREQ([2.69])
AC_INIT([DRCPD], [0.1], [r.tiemann@ta-hifi.com])
AC_CONFIG_AUX_DIR([config])
AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([drcpd.cc])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_USE_SYSTEM_EXTENSIONS()

AC_ARG_VAR([CWARNINGS], [strict set of C compiler warnings])
AC_ARG_VAR([CXXWARNINGS], [strict set of C++ compiler warnings])
AC_ARG_VAR([CRELAXEDWARNINGS], [options for relaxing some C compiler warnings for various files])
AC_ARG_VAR([CXXRELAXEDWARNINGS], [options for relaxing some C++ compiler warnings for various files])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_AR
AC_PROG_LIBTOOL
AM_GNU_GETTEXT_VERSION([0.18.3])
AM_GNU_GETTEXT([external])
PKG_PROG_PKG_CONFIG([0.26])
AC_CHECK_PROGS([MARKDOWN], [markdown])
AC_CHECK_PROGS([DOXYGEN], [doxygen])
AC_CHECK_PROGS([GDBUS_CODEGEN], [gdbus-codegen])
AC_CHECK_CUTTER
AC_CHECK_CPPCUTTER
AC_CHECK_GCUTTER
AC_CHECK_COVERAGE

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([libintl.h locale.h stdlib.h])

save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$pkg_cv_CUTTER_CFLAGS"
AC_CHECK_HEADER([cutter.h], [], [ac_cv_use_cutter=no])
CPPFLAGS="$save_CPPFLAGS"

AC_LANG_PUSH([C++])
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$pkg_cv_CPPCUTTER_CFLAGS"
AC_CHECK_HEADER([cppcutter.h], [], [ac_cv_use_cutter=no])
CPPFLAGS="$save_CPPFLAGS"
AC_LANG_POP([C++])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL

# Checks for library functions.
AC_CHECK_FUNCS([setenv setlocale])

AM_CONDITIONAL([WITH_CUTTER], [test "x$ac_cv_use_cutter" = "xyes"])

AC_CONFIG_FILES([Makefile tests/Makefile po/Makefile.in Doxyfile])
AC_CONFIG_FILES([tests/run_test.sh], [chmod +x tests/run_test.sh])
AC_OUTPUT
