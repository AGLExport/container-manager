/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	manager.h
 * @brief	container management daemon global config data
 */
#ifndef MANAGER_H
#define MANAGER_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include "list.h"

//-----------------------------------------------------------------------------
// Manager config ---------------------------------
struct s_container_manager_bridge_config {
	struct dl_list list;
	char *name;	 		/**< ethernet bridge name */
	//--- internal control data
};
typedef struct s_container_manager_bridge_config container_manager_bridge_config_t;

struct s_container_manager_config {
	char *configdir;	//**< container config directory */
	struct dl_list bridgelist;
};
typedef struct s_container_manager_config container_manager_config_t;

//-----------------------------------------------------------------------------
#endif //#ifndef MANAGER_H
