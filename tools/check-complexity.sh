#!/bin/sh

complexity --histogram --score --thresh=3 \
	include/libredundancyfileop.h \
	\
	lib/crc16.h \
	lib/fileop.c \
	lib/fileop.h \
	lib/file-util.c \
	lib/file-util.h \
	lib/libredundancyfileop.c \
	lib/static-configurator.c \
	lib/static-configurator.h


