AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

bin_PROGRAMS = \
	parser_test

parser_test_SOURCES = \
	parser/interface_test.cpp

# options
# Additional library
LDADD = \
	-lrt -lpthread \
	@LIBCJSON_LIBS@ \
	@GTEST_MAIN_LIBS@ \
	@GMOCK_MAIN_LIBS@

interface_test_LDADD = \
	${LDADD}

# C compiler options
CFLAGS = \
	-g \
	-fsanitize=address -coverage \
	-I$(top_srcdir)/3rdparty/wpa-supplicant \
	-I$(top_srcdir)/src/parser \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/include \
	@LIBCJSON_CFLAGS@ \
	@GTEST_MAIN_CFLAGS@ \
	@GMOCK_MAIN_CFLAGS@
	-D_GNU_SOURCE

# C++ compiler options
CXXFLAGS = \
	-g \
	-fpermissive \
	-std=c++11 -fno-exceptions \
	-fsanitize=address -coverage \
	-I$(top_srcdir)/3rdparty/wpa-supplicant \
	-I$(top_srcdir)/src/parser \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/include \
	@LIBCJSON_CFLAGS@ \
	@GTEST_MAIN_CFLAGS@ \
	@GMOCK_MAIN_CFLAGS@
	-D_GNU_SOURCE

# Linker options
LDFLAGS =


# configure option 
if ENABLE_ADDRESS_SANITIZER
CFLAGS   += -fsanitize=address
endif

if ENABLE_GCOV
CFLAGS   += -coverage
endif



if ENABLE_PRINTFDEBUG
CFLAGS   += -D_PRINTF_DEBUG_
CXXFLAGS += -D_PRINTF_DEBUG_
endif

if ENABLE_NOTARGET
CFLAGS   += -D_NOTARGET_
CXXFLAGS += -D_NOTARGET_
endif

CLEANFILES = *.gcda *.gcno
