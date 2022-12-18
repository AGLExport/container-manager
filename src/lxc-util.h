/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	lxc-util.h
 * @brief	lxc utility header
 */
#ifndef LXC_UTIL_H
#define LXC_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include "container.h"
#include <lxc/lxccontainer.h>

//-----------------------------------------------------------------------------
int lxcutil_create_instance(container_config_t *cc);
int lxcutil_container_shutdown(container_config_t *cc);
int lxcutil_container_forcekill(container_config_t *cc);
int lxcutil_release_instance(container_config_t *cc);

int lxcutil_dynamic_device_add_to_guest(container_config_t *cc, dynamic_device_elem_data_t *dded, int mode);
int lxcutil_dynamic_device_remove_from_guest(container_config_t *cc, dynamic_device_elem_data_t *dded, int mode);
int lxcutil_dynamic_networkif_add_to_guest(container_config_t *cc, container_dynamic_netif_elem_t *cdne);

//-----------------------------------------------------------------------------
#endif //#ifndef LXC_UTIL_H
