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

struct s_container_config;
typedef struct s_container_config container_config_t;

struct s_container_manager_role_elem {
	struct dl_list list;
	container_config_t *cc;	 		/**< pointer to container config */
	//--- internal control data
};
typedef struct s_container_manager_role_elem container_manager_role_elem_t;

struct s_container_manager_role_config {
	struct dl_list list;
	char *name;	 		/**< role name */
	//--- internal control data
	struct dl_list container_list;
};
typedef struct s_container_manager_role_config container_manager_role_config_t;

struct s_container_manager_config {
	char *configdir;	//**< container config directory */
	struct dl_list bridgelist;
	//--- internal control data
	struct dl_list role_list;
};
typedef struct s_container_manager_config container_manager_config_t;

//-----------------------------------------------------------------------------
#endif //#ifndef MANAGER_H
