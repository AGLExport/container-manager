AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = libs

# fsck plugin
plugindir = ${libdir}/container-manager

plugin_LTLIBRARIES = cm-worker-fsck.la
cm_worker_fsck_la_SOURCES = \
	fsck-plugin.c

# C compiler options
cm_worker_fsck_la_CFLAGS = \
	-g -Wall -Wno-unused-but-set-variable \
	-I$(top_srcdir)/include \
	-D_GNU_SOURCE

# C++ compiler options
cm_worker_fsck_la_CXXFLAGS = \
	-g -Wall -Wno-unused-but-set-variable \
	-I$(top_srcdir)/include \
	-D_GNU_SOURCE

cm_worker_fsck_la_LIBADD = \
	libs/libcmworker.a

# Linker options
cm_worker_fsck_la_LDFLAGS = \
	-module -avoid-version -shared \
	-fvisibility=hidden

# configure option
if ENABLE_ADDRESS_SANITIZER
CFLAGS   += -fsanitize=address
endif

if ENABLE_GCOV
CFLAGS   += -coverage
endif

CFLAGS   += -DCM_CRITICAL_ERROR_OUT_STDERROR
CXXFLAGS += -DCM_CRITICAL_ERROR_OUT_STDERROR

if ENABLE_PRINTFDEBUG
CFLAGS   += -D_PRINTF_DEBUG_
CXXFLAGS += -D_PRINTF_DEBUG_
endif

CLEANFILES = *.gcda *.gcno
