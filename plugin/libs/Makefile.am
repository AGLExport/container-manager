AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

# plugin support library
noinst_LIBRARIES = \
	libcmworker.a

libcmworker_a_SOURCES = \
	cm-worker-utils.c

# C compiler options
libcmworker_a_CFLAGS = \
	-g -Wall -Wno-unused-but-set-variable \
	-fPIC -DPIC \
	-D_GNU_SOURCE

# C++ compiler options
libcmworker_a_CXXFLAGS = \
	-g -Wall -Wno-unused-but-set-variable \
	-fPIC -DPIC \
	-D_GNU_SOURCE

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
