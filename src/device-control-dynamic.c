/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	device-control-dynamic.c
 * @brief	device control sub block for dynamic device management.
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
 * Setup dynamic device manager.
 * This function initialize to udevmonitor sub block and netifmonitor sub block.
 *
 * @param [out]	pddm	Double pointer to get constructed dynamic_device_manager_t.
 * @param [in]	cci		Pointer to container_control_interface_t, it use for notification from dynamic device sub blocks.
 * @param [in]	event	A sd event. (main event loop)
 * @return int
 * @retval  0	Success to setup dynamic device manager.
 * @retval -1	Critical error.
 */
int devc_device_manager_setup(dynamic_device_manager_t **pddm, container_control_interface_t *cci, sd_event *event)
{
	int ret = 1;
	int result = -1;
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
 * @retval  0	Success to cleanup.
 * @retval  -1	Critical error. (Reserve)
 */
int devc_device_manager_cleanup(dynamic_device_manager_t *ddm)
{
	(void)netifmonitor_cleanup(ddm);

	(void)udevmonitor_cleanup(ddm);

	free(ddm);

	return 0;
}
