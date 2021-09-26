AC_PREREQ([2.69])
AC_INIT([container-manager], [0.1])
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CONFIG_SRCDIR([src/container-manager.cpp])
AC_CONFIG_HEADERS([include/config.h])

AC_CONFIG_MACRO_DIR([m4])

AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [Enable debuging function (default is no)])],
  [:],
  [enable_debug=no])
AM_CONDITIONAL([ENABLE_DEBUG], [test "$enable_debug" = "yes"])

AC_ARG_ENABLE([debuglog],
  [AS_HELP_STRING([--enable-debuglog], [Enable debug log (default is no)])],
  [:],
  [enable_debuglog=no])
AM_CONDITIONAL([ENABLE_DEBUGLOG], [test "$enable_debuglog" = "yes"])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

AC_DISABLE_STATIC

LT_INIT
AM_PROG_LIBTOOL 

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([LIBMNL], [libmnl])
PKG_CHECK_MODULES([LIBSYSTEMD], [libsystemd])

AC_CONFIG_FILES([ Makefile src/Makefile ])
AC_OUTPUT

