/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	config.h
 * @brief	config utility header
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
#endif //#ifndef CONTAINER_CONFIGL_H
