/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cm-utils.c
 * @brief	container manager utility functions
 */
#include "cm-log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int g_log_output_level = CM_LOGLEVEL_CRITICAL_INFO;
/**
 * Container Manager log.
 * This function supports log level filtered log output to stderr.
 *
 * @param [in]	level	Log level
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
void cm_log(int level, const char *format, ...)
{
	if (level <= g_log_output_level) {
		va_list args;

		va_start(args, format);
		(void) vfprintf(stderr, format, args);
		va_end(args);
	}
}
