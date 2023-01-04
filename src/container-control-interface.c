/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-control-interface.c
 * @brief	This file include implementation for container manager control interface.
 */

#include "container-control-interface.h"
#include "container-control-internal.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <sys/sysmacros.h>

#include <sys/mount.h>

#include "cm-utils.h"
#include "lxc-util.h"
#include "container-config.h"

static int container_mngsm_device_updated(struct s_container_control_interface *cci);
static int container_mngsm_netif_updated(struct s_container_control_interface *cci);
static int container_mngsm_system_shutdown(struct s_container_control_interface *cci);

/**
 * Container management state machine cleanup.
 *
 * @param [in]	cs	Instance of containers_t
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
int container_mngsm_interface_get(container_control_interface_t **pcci, containers_t *cs)
{
	struct s_container_control_interface *cci = NULL;

	if (pcci == NULL || cs == NULL)
		return -2;

	if (cs->cci == NULL) {
		cci = (struct s_container_control_interface*)malloc(sizeof(struct s_container_control_interface));
		if (cci == NULL)
			goto err_return;

		memset(cci, 0, sizeof(struct s_container_control_interface));

		cci->mngsm = (void*)cs->cms;
		cci->device_updated = container_mngsm_device_updated;
		cci->netif_updated = container_mngsm_netif_updated;
		cci->system_shutdown = container_mngsm_system_shutdown;

		cs->cci = (container_control_interface_t*)cci;
	}

	(*pcci) = (struct s_container_control_interface*)cs->cci;

	return 0;

err_return:

	return -1;
}
/**
 * Container management state machine cleanup.
 *
 * @param [in]	cs	Instance of containers_t
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
int container_mngsm_interface_free(containers_t *cs)
{

	if (cs == NULL)
		return -2;

	free(cs->cci);
	cs->cci = NULL;

	return 0;
}
/**
 * Device update notification to container manager state machine
 *
 * @param [in]	cci	Interface struct
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
static int container_mngsm_device_updated(struct s_container_control_interface *cci)
{
	struct s_container_mngsm *cm = NULL;
	container_mngsm_notification_t command;
	ssize_t ret = -1;

	if (cci == NULL)
		return -1;

	cm = (struct s_container_mngsm*)cci->mngsm;

	memset(&command, 0, sizeof(command));

	command.header.command = CONTAINER_MNGSM_COMMAND_DEVICEUPDATED;

	ret = write(cm->secondary_fd, &command, sizeof(command));
	if (ret != sizeof(command))
		return -1;

	return 0;
}
/**
 * Network interface update notification to container manager state machine
 *
 * @param [in]	cci	Interface struct
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
static int container_mngsm_netif_updated(struct s_container_control_interface *cci)
{
	struct s_container_mngsm *cm = NULL;
	container_mngsm_notification_t command;
	ssize_t ret = -1;

	if (cci == NULL)
		return -1;

	cm = (struct s_container_mngsm*)cci->mngsm;

	memset(&command, 0, sizeof(command));

	command.header.command = CONTAINER_MNGSM_COMMAND_NETIFUPDATED;

	ret = write(cm->secondary_fd, &command, sizeof(command));
	if (ret != sizeof(command))
		return -1;

	return 0;
}
/**
 * Network interface update notification to container manager state machine
 *
 * @param [in]	cci	Interface struct
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
static int container_mngsm_system_shutdown(struct s_container_control_interface *cci)
{
	struct s_container_mngsm *cm = NULL;
	container_mngsm_notification_t command;
	ssize_t ret = -1;

	if (cci == NULL)
		return -1;

	cm = (struct s_container_mngsm*)cci->mngsm;

	memset(&command, 0, sizeof(command));

	command.header.command = CONTAINER_MNGSM_COMMAND_SYSTEM_SHUTDOWN;

	ret = write(cm->secondary_fd, &command, sizeof(command));
	if (ret != sizeof(command))
		return -1;

	return 0;
}