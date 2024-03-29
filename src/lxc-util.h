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
 * @struct  s_lxcutil_dynamic_device_request
 * @brief   The parameter of dynamic device operation request.
 */
struct s_lxcutil_dynamic_device_request {
    int operation;	        /**< operation mode. 1: add, 2: remove. */
    int devtype;	        /**< char or block device. DEVNODE_TYPE_CHR or DEVNODE_TYPE_BLK. */
    int dev_major;          /**< major number of device. */
    int dev_minor;          /**< minor number of device. */
    int is_create_node;     /**< create device node or not. 1: create node, 0: note create.*/
    int is_allow_device;    /**< allow/deny device or not. 1:yes, 0:no. */
    const char *devnode;    /**< device node name. */
    const char *permission; /**< access permission fo device to use device allow/deny setting. */
};
typedef struct s_lxcutil_dynamic_device_request lxcutil_dynamic_device_request_t;	/**< typedef for struct s_lxcutil_dynamic_device_request. */

//-----------------------------------------------------------------------------
int lxcutil_create_instance(container_config_t *cc);
int lxcutil_container_shutdown(container_config_t *cc);
int lxcutil_container_forcekill(container_config_t *cc);
int lxcutil_release_instance(container_config_t *cc);
pid_t lxcutil_get_init_pid(container_config_t *cc);

int lxcutil_dynamic_device_operation(container_config_t *cc, lxcutil_dynamic_device_request_t *lddr);

int lxcutil_dynamic_networkif_add_to_guest(container_config_t *cc, container_dynamic_netif_elem_t *cdne);

int lxcutil_dynamic_mount_to_guest(container_config_t *cc, const char *host_path, const char *guest_path);
//-----------------------------------------------------------------------------
#endif //#ifndef LXC_UTIL_H
