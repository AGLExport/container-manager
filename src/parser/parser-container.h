/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	parser-container.h
 * @brief	container config parser header
 */
#ifndef PARSER_CONTAINER_H
#define PARSER_CONTAINER_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include "container.h"


//-----------------------------------------------------------------------------
int cmparser_create_from_file(container_config_t **cc, const char *file);
void cmparser_release_config(container_config_t *cc);

//-----------------------------------------------------------------------------
#endif //#ifndef PARSER_CONTAINER_H
