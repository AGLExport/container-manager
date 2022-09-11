/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-control-interface.h
 * @brief	container control interface header
 */
#ifndef CONTAINER_CONTROL_INTERFACE_H
#define CONTAINER_CONTROL_INTERFACE_H
//-----------------------------------------------------------------------------
#include <stdint.h>


//-----------------------------------------------------------------------------
struct s_container_control_interface {
	void *mngsm;

	int (*device_updated)(struct s_container_control_interface *cci);
	int (*netif_updated)(struct s_container_control_interface *cci);
};
typedef struct s_container_control_interface container_control_interface_t;

//-----------------------------------------------------------------------------
#endif //#ifndef CONTAINER_CONTROL_INTERFACE_H
