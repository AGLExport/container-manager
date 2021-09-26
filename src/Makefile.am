AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

bin_PROGRAMS = container-manager

container_manager_SOURCES = \
		CEventLoop.cxx \
		container-manager.cpp 

container_manager_LDADD = \
		-lrt -lpthread \
		@LIBMNL_LIBS@ \
		@LIBSYSTEMD_LIBS@

container_manager_CFLAGS = \
		-I$(top_srcdir)/include \
		-g \
		@LIBMNL_LIBS@ \
		@LIBSYSTEMD_LIBS@

container_manager_CXXFLAGS = \
		-I$(top_srcdir)/include \
		-std=c++11 -g \
		@LIBMNL_LIBS@ \
		@LIBSYSTEMD_LIBS@

container_manager_LDFLAGS = \
		-lrt -lpthread \
		@LIBMNL_LIBS@ \
		@LIBSYSTEMD_LIBS@


container_manager_CFLAGS   += @LIBMNL_CFLAGS@ @LIBSYSTEMD_CFLAGS@
container_manager_CXXFLAGS += @LIBMNL_CFLAGS@ @LIBSYSTEMD_CFLAGS@

if ENABLE_DEBUG
container_manager_CFLAGS   += -DENABLE_DEBUG
container_manager_CXXFLAGS += -DENABLE_DEBUG
endif

if ENABLE_DEBUGLOG
container_manager_CFLAGS   += -DENABLE_DEBUGLOG
container_manager_CXXFLAGS += -DENABLE_DEBUGLOG
endif

