/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	parser-common.h
 * @brief	container config parser common function header
 */
#ifndef PARSER_COMMON_H
#define PARSER_COMMON_H
//-----------------------------------------------------------------------------
#include <stdint.h>

//-----------------------------------------------------------------------------
char *cmparser_read_jsonstring(const char *file);
void cmparser_release_jsonstring(char *jsonstring);

//-----------------------------------------------------------------------------
#endif //#ifndef PARSER_COMMON_H
