/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	lxc-util.h
 * @brief	The header for LXC control interface.
 */
#ifndef LXC_UTIL_H
#define LXC_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include "container.h"

//-----------------------------------------------------------------------------
/**
 * @struct
 * @brief
 */
struct s_lxcutil_dynamic_device_request {
    int operation;	    /**< operation mode. 1: add, 2: remove. */
    int devtype;	    /**< char or block device. DEVNODE_TYPE_CHR or DEVNODE_TYPE_BLK. */
    int dev_major;      /**< major number of device. */
    int dev_minor;      /**< minor number of device. */
    int is_create_node; /**< create device node or not. 1: create node, 0: note create.*/
    const char *devnode;      /**< device node name. */
    const char *permission;
};
typedef struct s_lxcutil_dynamic_device_request lxcutil_dynamic_device_request_t;	/**< typedef for struct s_lxcutil_dynamic_device_request. */

//-----------------------------------------------------------------------------
int lxcutil_create_instance(container_config_t *cc);
int lxcutil_container_shutdown(container_config_t *cc);
int lxcutil_container_forcekill(container_config_t *cc);
int lxcutil_release_instance(container_config_t *cc);

int lxcutil_dynamic_device_operation(container_config_t *cc, lxcutil_dynamic_device_request_t *lddr);

//int lxcutil_dynamic_device_add_to_guest(container_config_t *cc, dynamic_device_elem_data_t *dded, int mode);
//int lxcutil_dynamic_device_remove_from_guest(container_config_t *cc, dynamic_device_elem_data_t *dded, int mode);
int lxcutil_dynamic_networkif_add_to_guest(container_config_t *cc, container_dynamic_netif_elem_t *cdne);

//-----------------------------------------------------------------------------
#endif //#ifndef LXC_UTIL_H
