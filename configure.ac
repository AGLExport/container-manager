# autoconf sample

AC_PREREQ([2.69])

AC_INIT([exsample], [1.0])

AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CONFIG_SRCDIR([src/container-manager.c])

AC_CONFIG_HEADERS([include/config.h])

AC_CONFIG_MACRO_DIR([m4])


AC_ARG_ENABLE([address-sanitizer],
  [AS_HELP_STRING([--enable-address-sanitizer], [Enable address sanitizer in build time (for debug, default is no)])],
  [:],
  [enable_address_sanitizer=no])
AM_CONDITIONAL([ENABLE_ADDRESS_SANITIZER], [test "$enable_address_sanitizer" = "yes"])

AC_ARG_ENABLE([gcov],
  [AS_HELP_STRING([--enable-gcov], [Enable gcov in build time (for debug, default is no)])],
  [:],
  [enable_gcov=no])
AM_CONDITIONAL([ENABLE_GCOV], [test "$enable_gcov" = "yes"])

AC_ARG_ENABLE([test],
  [AS_HELP_STRING([--enable-test], [Enable unit test build (requir to gtest and gmock, default is no])],
  [:],
  [enable_test=no])
AM_CONDITIONAL([ENABLE_TEST], [test "$enable_test" = "yes"])


AC_ARG_ENABLE([printfdebug],
  [AS_HELP_STRING([--enable-printfdebug], [Enable user printf debug (default is no)])],
  [:],
  [enable_printfdebug=no])
AM_CONDITIONAL([ENABLE_PRINTFDEBUG], [test "$enable_printfdebug" = "yes"])

AC_ARG_ENABLE([notarget],
  [AS_HELP_STRING([--enable-notarget], [Enable non terget behavior (default is no)])],
  [:],
  [enable_printfdebug=no])
AM_CONDITIONAL([ENABLE_NOTARGET], [test "$enable_notarget" = "yes"])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

AC_DISABLE_STATIC

# Checks for libraries.
LT_INIT
AM_PROG_LIBTOOL

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([LIBSYSTEMD], [libsystemd])
PKG_CHECK_MODULES([LIBUDEV], [libudev])
PKG_CHECK_MODULES([LIBMNL], [libmnl])
PKG_CHECK_MODULES([LIBCJSON], [libcjson])
PKG_CHECK_MODULES([LIBLXC], [lxc])
PKG_CHECK_MODULES([LIBBLKID], [blkid])
PKG_CHECK_MODULES([GTEST_MAIN], [gtest_main], , enable_test=no)
PKG_CHECK_MODULES([GMOCK_MAIN], [gmock_main], , enable_test=no)



# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
				src/Makefile
        src/command/Makefile
        plugin/Makefile
        plugin/libs/Makefile
				test/unit/Makefile ])
AC_OUTPUT

