/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cm-utils.h
 * @brief	container manager utility header
 */
#ifndef CM_LOG_H
#define CM_LOG_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>

//-----------------------------------------------------------------------------
#define CM_LOGLEVEL_CRITICAL_ERROR	(0)	// Log level for the critical error. Could not operate.
#define CM_LOGLEVEL_CRITICAL_INFO	(1)	// Log level for the critical info. Do recovery operation.
#define CM_LOGLEVEL_ERROR			(10)	// Log level for the generic error.
#define CM_LOGLEVEL_WARNING			(20)	// Log level for the generic warning.
#define CM_LOGLEVEL_INFO			(30)	// Log level for the generic info.
#define CM_LOGLEVEL_DEBUG			(40)	// Log level for the debug.
#define CM_LOGLEVEL_TRACE			(50)	// Log level for the trace.

//-----------------------------------------------------------------------------
void cm_log(int level, const char *format, ...);

//-----------------------------------------------------------------------------
#endif //#ifndef CM_LOG_H
