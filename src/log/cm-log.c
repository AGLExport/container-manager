/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cm-utils.c
 * @brief	container manager utility functions
 */
#include "cm-log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_log_output_level = CM_LOGLEVEL_CRITICAL_INFO;

/**
 * Test whether a log level is enabled.
 *
 * @param [in] level Log level.
 * @return 1 when the log level is enabled, zero otherwise.
 */
int cm_log_enabled(int level)
{
	int result = 0;

	if (level <= g_log_output_level) {
		result = 1;
	}
	return result;
}

int cm_log_set_minimum_level(int level)
{
	int old = g_log_output_level;
	g_log_output_level = level;

	return old;
}
/**
 * Set the log output threshold from the CM_LOG_LEVEL environment variable.
 * Unset or empty values leave the current threshold unchanged.
 *
 * @return 0 on success (including unset or empty), -1 for an invalid value.
 *         An invalid value leaves the current threshold unchanged.
 */
int cm_log_set_level_from_env(void)
{
	static const struct {
		const char *name;
		int level;
	} levels[] = {
		{ "critical-error", CM_LOGLEVEL_CRITICAL_ERROR },
		{ "critical-info", CM_LOGLEVEL_CRITICAL_INFO },
		{ "error", CM_LOGLEVEL_ERROR },
		{ "warning", CM_LOGLEVEL_WARNING },
		{ "info", CM_LOGLEVEL_INFO },
		{ "debug", CM_LOGLEVEL_DEBUG },
		{ "trace", CM_LOGLEVEL_TRACE },
	};
	const char *value = getenv("CM_LOG_LEVEL");
	size_t i;

	if (value == NULL || value[0] == '\0') {
		return 0;
	}

	for (i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
		if (strncmp(value, levels[i].name, strlen(levels[i].name)) == 0) {
			(void) cm_log_set_minimum_level(levels[i].level);
			return 0;
		}
	}

	return -1;
}

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
	if (cm_log_enabled(level)) {
		va_list args;

		va_start(args, format);
		(void) vfprintf(stderr, format, args);
		va_end(args);
	}
}
