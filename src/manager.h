/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	manager.h
 * @brief	container management daemon grobal config data 
 */
#ifndef MAINAGER_H
#define MAINAGER_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include "list.h"

//-----------------------------------------------------------------------------
// Manager config ---------------------------------
struct s_container_manager_bridge_config {
	struct dl_list list;
	char *name;	 		/**< ethernet bridge name */
	//--- interal control data
};
typedef struct s_container_manager_bridge_config container_manager_bridge_config_t;

struct s_container_manager_config {
	char *configdir;	//**< container config directry */
	struct dl_list bridgelist;
};
typedef struct s_container_manager_config container_manager_config_t;

//-----------------------------------------------------------------------------
#endif //#ifndef MAINAGER_H
