/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-config.h
 * @brief	Header file for the implementation for create and release top level container configs.
 */
#ifndef CONTAINER_CONFIG_H
#define CONTAINER_CONFIG_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include "container.h"

//-----------------------------------------------------------------------------
containers_t *create_container_configs(const char *config_dir);
int release_container_configs(containers_t *cs);

//-----------------------------------------------------------------------------
#endif //#ifndef CONTAINER_CONFIG_H
