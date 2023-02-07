/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-control-interface.h
 * @brief	The header file for container control interface.
 */
#ifndef CONTAINER_CONTROL_INTERFACE_H
#define CONTAINER_CONTROL_INTERFACE_H
//-----------------------------------------------------------------------------
#include <stdint.h>

//-----------------------------------------------------------------------------
/**
 * @struct	s_container_control_interface
 * @brief	Interface container structure for container manager state machine interface.  This structure carry function pointer of interface and some data.
 */
struct s_container_control_interface {
	void *mngsm;	/**< Pointer to parent container manager state machine. */

	int (*netif_updated)(struct s_container_control_interface *cci);	/**< Function pointer for network interface update notification interface. */

	int (*system_shutdown)(struct s_container_control_interface *cci);	/**< Function pointer for received shutdown request notification interface. */
};
typedef struct s_container_control_interface container_control_interface_t;	/**< typedef for struct s_container_control_interface. */

//-----------------------------------------------------------------------------
#endif //#ifndef CONTAINER_CONTROL_INTERFACE_H
