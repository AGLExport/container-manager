AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

bin_PROGRAMS = cmcontrol

cmcontrol_SOURCES = \
	cmcontrol.c


# options
# Additional library
cmcontrol_LDADD =
	-lrt -lpthread

# C compiler options
cmcontrol_CFLAGS = \
	-g -Wall \
	-I$(top_srcdir)/include \
	-D_GNU_SOURCE

# C++ compiler options
cmcontrol_CXXFLAGS = \
	-g -Wall \
	-I$(top_srcdir)/include \
	-D_GNU_SOURCE

# Linker options
cmcontrol_LDFLAGS =


# configure option
if ENABLE_ADDRESS_SANITIZER
CFLAGS   += -fsanitize=address
endif

if ENABLE_GCOV
CFLAGS   += -coverage
endif

cmcontrol_CFLAGS   += -DCM_CRITICAL_ERROR_OUT_STDERROR
cmcontrol_CXXFLAGS += -DCM_CRITICAL_ERROR_OUT_STDERROR

if ENABLE_PRINTFDEBUG
cmcontrol_CFLAGS   += -D_PRINTF_DEBUG_
cmcontrol_CXXFLAGS += -D_PRINTF_DEBUG_
endif

if ENABLE_NOTARGET
cmcontrol_CFLAGS   += -D_NOTARGET_
cmcontrol_CXXFLAGS += -D_NOTARGET_
endif

CLEANFILES = *.gcda *.gcno
