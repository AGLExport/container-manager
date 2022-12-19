/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	device-control.c
 * @brief	device control block for container manager
 */

#include "device-control.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/sysmacros.h>

#include "cm-utils.h"
#include "udev-util.h"
#include "net-util.h"

/**
 * Start up time device initialization
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Device scan critical error.
 */
int devc_device_manager_setup(dynamic_device_manager_t **pddm, container_control_interface_t *cci, sd_event *event)
{
	int num;
	int ret = 1;
	int result = -1;
	container_config_t *cc = NULL;
	dynamic_device_manager_t *ddm = NULL;

	ddm = (dynamic_device_manager_t*)malloc(sizeof(dynamic_device_manager_t));
	if (ddm == NULL)
		return -1;

	ret = udevmonitor_setup(ddm, cci, event);
	if (ret < 0)
		goto err_ret;

	ret = netifmonitor_setup(ddm, cci, event);
	if (ret < 0)
		goto err_ret;

	(*pddm) = ddm;

	return 0;

err_ret:

	return result;
}
/**
 * Cleanup device manager
 *
 * @param [in]	ddm	Pointer to dynamic_device_manager_t
 * @return int
 * @retval  0 Success.
 */
int devc_device_manager_cleanup(dynamic_device_manager_t *ddm)
{
	(void)netifmonitor_cleanup(ddm);

	(void)udevmonitor_cleanup(ddm);

	free(ddm);

	return 0;
}
