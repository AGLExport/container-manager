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
/**
 * @struct	s_container_manager_bridge_config
 * @brief	The data structure for network bridge.  It use bridge interface creation at boot time of container manager.
 */
struct s_container_manager_bridge_config {
	struct dl_list list;	/**< Double link list header. */
	char *name;	 			/**< Ethernet bridge name */
	//--- internal control data
};
typedef struct s_container_manager_bridge_config container_manager_bridge_config_t;	/**< typedef for struct s_container_manager_bridge_config. */

/**
 * @struct	s_container_manager_operation_storage
 * @brief	The data storage for manager operations.
 */
struct s_container_manager_operation_storage;
typedef struct s_container_manager_operation_storage container_manager_operation_storage_t;	/**< typedef for struct s_container_manager_operation_storage. */

/**
 * @struct	s_container_manager_operation
 * @brief	The data structure for manager operation.
 */
struct s_container_manager_operation {
	container_manager_operation_storage_t *storage;
};
typedef struct s_container_manager_operation container_manager_operation_t;	/**< typedef for struct s_container_manager_operation. */

struct s_container_config;
typedef struct s_container_config container_config_t;

/**
 * @struct	s_container_manager_role_elem
 * @brief	The data structure for container role management.  This data use to manage which guest container has which role.
 */
struct s_container_manager_role_elem {
	struct dl_list list;	/**< Double link list header. */
	container_config_t *cc;	/**< pointer to container config */
	//--- internal control data
};
typedef struct s_container_manager_role_elem container_manager_role_elem_t;	/**< typedef for struct s_container_manager_role_elem. */

/**
 * @struct	s_container_manager_role_config
 * @brief	The data structure for container role management.  This data show role.  The container_list of this is linking all guest container that has same as own role.
 */
struct s_container_manager_role_config {
	struct dl_list list;			/**< Double link list header. */
	char *name;	 					/**< Role name. */
	//--- internal control data
	struct dl_list container_list;	/**< Double link list for struct s_container_config . */
};
typedef struct s_container_manager_role_config container_manager_role_config_t;	/**< typedef for struct s_container_manager_role_config. */

/**
 * @struct	s_container_manager_config
 * @brief	Top level data for container manager config.
 */
struct s_container_manager_config {
	char *configdir;			/**< Guest container config directory */
	struct dl_list bridgelist;	/**< Double link list for s_container_manager_bridge_config. */

	//--- internal control data
	struct dl_list role_list;	/**< Double link list for s_container_manager_role_config. */
};
typedef struct s_container_manager_config container_manager_config_t;	/**< typedef for struct s_container_manager_config. */

//-----------------------------------------------------------------------------
#endif //#ifndef MANAGER_H
