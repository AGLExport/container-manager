/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	parser.h
 * @brief	container config parser header
 */
#ifndef PARSER_MANAGER_H
#define PARSER_MANAGER_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include "manager.h"


//-----------------------------------------------------------------------------
int cmparser_manager_create_from_file(container_manager_config_t **cm, const char *file);
void cmparser_manager_release_config(container_manager_config_t *cm);

//-----------------------------------------------------------------------------
#endif //#ifndef PARSER_MANAGER_H
