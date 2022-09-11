/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	device-control.c
 * @brief	device control block for container manager
 */

#include "container-control.h"
#include "container-control-internal.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>

#include "cm-utils.h"
#include "lxc-util.h"
#include "container-config.h"
#include "device-control.h"



/**
 * Guest monitoring notification to container manager state machine
 *
 * @param [in]	cci	Interface struct
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
static int container_monitor_state_change(containers_t *cs, int status, int num)
{
	struct s_container_mngsm *cm = NULL;
	container_mngsm_guest_status_change_t command;
	ssize_t ret = -1;

	if (cs == NULL)
		return -1;

	cm = (struct s_container_mngsm*)cs->cms;

	memset(&command, 0, sizeof(command));

	command.header.command = CONTAINER_MNGSM_COMMAND_GUEST_STATUS_CHANGE;
	command.data.new_status = status;
	command.data.container_number = num;

	ret = write(cm->secondary_fd, &command, sizeof(command));
	if (ret != sizeof(command))
		return -1;

	return 0;
}
/**
 * Event handler for pidfd monitor
 *
 * @param [in]	event		Socket event source object
 * @param [in]	fd			File discriptor for socket session
 * @param [in]	revents		Active event (epooll)
 * @param [in]	userdata	Pointer to data_pool_service_handle
 * @return int	 0 success
 *				-1 internal error
 */
static int container_monitor_pidfd_handler(sd_event_source *event, int fd, uint32_t revents, void *userdata)
{
	containers_t *cs = NULL;
	container_config_t *cc = NULL;
	int ret = -1;
	int num = 0;

	if (userdata == NULL) {
		//  Faile safe it unref.
		sd_event_source_disable_unref(event);
		return 0;
	}

	cs = (containers_t*)userdata;

	// pidfd is actived in process exit. Other case will not active
	num = cs->num_of_container;
	for(int i=0;i < num;i++) {
		cc = cs->containers[i];

		if (cc->runtime_stat.pidfd_source == event) {
			fprintf(stderr,"%s is exited\n", cc->name);
			ret = container_monitor_state_change(cs, CONTAINER_DEAD, i);
			//TODO error handling
		}
	}

	sd_event_source_disable_unref(event);

	return 0;
}
/**
 * Preprocess for container start base
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Arg error.
 * @retval -2 pidfd error.
 */
int container_monitor_addguest(containers_t *cs, container_config_t *cc)
{
	sd_event_source *pidfd_source = NULL;
	struct s_container_mngsm *cms = NULL;
	int ret = -1;
	int pidfd = -1;

	if (cs == NULL || cc == NULL)
		return -1;

	pidfd = cc->runtime_stat.lxc->init_pidfd(cc->runtime_stat.lxc);
	if (pidfd < 0)
		return -2;

	ret = sd_event_add_io(cs->event, &pidfd_source, pidfd, (EPOLLIN | EPOLLHUP | EPOLLERR), container_monitor_pidfd_handler, cs);
	if (ret < 0) {
		ret = -2;
		goto err_return;
	}

	cc->runtime_stat.pidfd_source = pidfd_source;

	return 0;

err_return:
	if (pidfd_source != NULL)
		(void)sd_event_source_disable_unref(pidfd_source);

	return -1;
}
/**
 * Preprocess for container start base
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 mount error.
 * @retval -2 Syscall error. 
 */
int container_monitor_removeguest(containers_t *cs, container_config_t *cc)
{
	if (cs == NULL || cc == NULL)
		return -1;


	return 0;
}

