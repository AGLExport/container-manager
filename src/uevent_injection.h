/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	uevent_injection.h
 * @brief	The header for uevent injection utility.
 */
#ifndef UEVENT_INJECTION_H
#define UEVENT_INJECTION_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include "container.h"

//-----------------------------------------------------------------------------
/**
 * @def	UEVENT_INJECTION_BUFFER_SIZE
 * @brief	uevent injection buffer size max. This value is based on UEVENT_BUFFER_SIZE at include/linux/kobject.h.
 */
#define UEVENT_INJECTION_BUFFER_SIZE    (2048)

/**
 * @struct	s_uevent_injection_message
 * @brief	The data structure for container root filesystem.  It's a part of s_container_baseconfig.
 */
struct s_uevent_injection_message {
	char head[UEVENT_INJECTION_BUFFER_SIZE];	        /**< rootfs file system type. */
    char message[UEVENT_INJECTION_BUFFER_SIZE];
};
typedef struct s_uevent_injection_message uevent_injection_message_t;	/**< typedef for struct s_container_baseconfig_rootfs. */


//-----------------------------------------------------------------------------
int uevent_injection_to_pid(pid_t target_pid, dynamic_device_elem_data_t *dded, char *action);

//-----------------------------------------------------------------------------
#endif //#ifndef UEVENT_INJECTION_H
