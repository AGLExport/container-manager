AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

noinst_LIBRARIES = libsocketcangw.a

libsocketcangw_a_SOURCES = \
	libsocketcangw.c

libsocketcangw_a_CFLAGS = \
	-g -Wall -Wno-unused-but-set-variable \
	-D_GNU_SOURCE

# configure option 
if ENABLE_ADDRESS_SANITIZER
CFLAGS   += -fsanitize=address
endif

if ENABLE_GCOV
CFLAGS   += -coverage
endif

CLEANFILES = *.gcda *.gcno