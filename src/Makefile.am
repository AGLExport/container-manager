AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = command

bin_PROGRAMS = containermanager

containermanager_SOURCES = \
    parser/parser-common.c \
    parser/parser-container.c \
    parser/parser-manager.c \
	cm-utils.c \
	block-util.c \
	lxc-util.c \
	net-util.c \
	signal-util.c \
	proc-util.c \
	uevent_injection.c \
	container-config.c \
	device-control-static.c \
	device-control-dynamic.c \
	device-control-dynamic-udev.c \
	container-control.c \
	container-control-interface.c \
	container-control-exec.c \
	container-control-monitor.c \
	container-external-interface.c \
	container-workqueue.c \
	container-workqueue-worker.c \
	container-manager.c


# options
# Additional library
containermanager_LDADD = \
	-lrt -lpthread \
	@LIBSYSTEMD_LIBS@ \
	@LIBUDEV_LIBS@ \
	@LIBMNL_LIBS@ \
	-lcjson \
	@LIBLXC_LIBS@ \
	@LIBBLKID_LIBS@

# C compiler options
containermanager_CFLAGS = \
	-g -Wall -Wno-unused-but-set-variable \
	-I$(top_srcdir)/3rdparty/wpa-supplicant \
	-I$(top_srcdir)/include \
	@LIBSYSTEMD_CFLAGS@ \
	@LIBUDEV_CFLAGS@ \
	@LIBMNL_CFLAGS@ \
	-I/usr/include/cjson \
	@LIBLXC_CFLAGS@ \
	@LIBBLKID_CFLAGS@ \
	-D_GNU_SOURCE

# C++ compiler options
containermanager_CXXFLAGS = \
	-g -Wall -Wno-unused-but-set-variable \
	-I$(top_srcdir)/3rdparty/wpa-supplicant \
	-I$(top_srcdir)/include \
	@LIBSYSTEMD_CFLAGS@ \
	@LIBUDEV_CFLAGS@ \
	@LIBMNL_CFLAGS@ \
	-I/usr/include/cjson \
	@LIBLXC_CFLAGS@ \
	@LIBBLKID_CFLAGS@ \
	-D_GNU_SOURCE

# Linker options
containermanager_LDFLAGS =


# configure option
if ENABLE_ADDRESS_SANITIZER
CFLAGS   += -fsanitize=address
endif

if ENABLE_GCOV
CFLAGS   += -coverage
endif

containermanager_CFLAGS   += -DCM_CRITICAL_ERROR_OUT_STDERROR
containermanager_CXXFLAGS += -DCM_CRITICAL_ERROR_OUT_STDERROR

if ENABLE_PRINTFDEBUG
containermanager_CFLAGS   += -D_PRINTF_DEBUG_
containermanager_CXXFLAGS += -D_PRINTF_DEBUG_
endif

if ENABLE_NOTARGET
containermanager_CFLAGS   += -D_NOTARGET_
containermanager_CXXFLAGS += -D_NOTARGET_
endif

CLEANFILES = *.gcda *.gcno
