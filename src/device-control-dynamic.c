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
#include "device-control-dynamic-udev.h"
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
int devc_device_manager_setup(containers_t *cs, sd_event *event)
{
	int ret = 1;
	int result = -1;
	dynamic_device_manager_t *ddm = NULL;

	ddm = (dynamic_device_manager_t*)malloc(sizeof(dynamic_device_manager_t));
	if (ddm == NULL)
		return -1;

	memset(ddm, 0, sizeof(dynamic_device_manager_t));

	cs->ddm = ddm;

	ret = device_control_dynamic_udev_setup(ddm, cs, event);
	if (ret < 0)
		goto err_ret;

/*
	ret = netifmonitor_setup(ddm, cci, event);
	if (ret < 0)
		goto err_ret;
*/

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
int devc_device_manager_cleanup(containers_t *cs)
{
	dynamic_device_manager_t *ddm = NULL;

	ddm = cs->ddm;
	if (ddm != NULL) {
		//(void)netifmonitor_cleanup(ddm);

		(void)device_control_dynamic_udev_cleanup(ddm);

		free(ddm);
	}
	cs->ddm = NULL;

	return 0;
}
