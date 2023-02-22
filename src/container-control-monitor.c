/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-control-monitor.c
 * @brief	This file include implementation for guest container monitoring feature.
 */

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
 * Guest container status change notification to container manager state machine from container monitor.
 *
 * @param [in]	cs		Pointer to containers_t.
 * @param [in]	status	New status for guest container.
 * @param [in]	num		Which guest container was changed state.
 * @return int
 * @retval  0 Success to send event.
 * @retval -1 Critical error for sending event.
 */
static int container_monitor_state_change(containers_t *cs, int status, int num)
{
	struct s_container_mngsm *cm = NULL;
	container_mngsm_guest_status_exit_t command;
	ssize_t ret = -1;

	if (cs == NULL) {
		return -1;
	}

	cm = (struct s_container_mngsm*)cs->cms;

	(void) memset(&command, 0, sizeof(command));

	command.header.command = CONTAINER_MNGSM_COMMAND_GUEST_EXIT;
	command.data.container_number = num;

	ret = write(cm->secondary_fd, &command, sizeof(command));
	if (ret != sizeof(command)) {
		return -1;
	}

	return 0;
}
/**
 * Event handler for pidfd monitor.
 * Container manager is monitoring to availability for guest container using pidfd.
 *
 * @param [in]	event		Socket event source object.
 * @param [in]	fd			File descriptor for socket session.
 * @param [in]	revents		Active event (epoll).
 * @param [in]	userdata	Pointer to containers_t.
 * @return int
 * @retval	0	Success to event handling.
 * @retval	-1	Internal error (Not use).
 */
static int container_monitor_pidfd_handler(sd_event_source *event, int fd, uint32_t revents, void *userdata)
{
	containers_t *cs = NULL;
	container_config_t *cc = NULL;
	int ret = -1;
	int num = 0;

	if (userdata == NULL) {
		//  Fail safe it unref.
		sd_event_source_disable_unref(event);
		return 0;
	}

	cs = (containers_t*)userdata;

	// pidfd is activated in process exit. Other case will not active
	num = cs->num_of_container;
	for(int i=0;i < num;i++) {
		cc = cs->containers[i];

		if (cc->runtime_stat.pidfd_source == event) {
			ret = container_monitor_state_change(cs, CONTAINER_DEAD, i);
			if (ret == 0) {
				sd_event_source_disable_unref(event);
				cc->runtime_stat.pidfd_source = NULL;
			}
			// if command send fail...
			// A main state machine event is highest priority. This event will retry after state machine event exced.
		}
	}

	return 0;
}
/**
 * Start guest container monitoring using container monitor.
 *
 * @param [in]	cs	Pointer ro containers_t.
 * @param [in]	cc	Pointer to container_config_t that show which guest start monitoring.
 * @return int
 * @retval  0 Success to start monitoring.
 * @retval -1 Argument error.
 * @retval -2 pidfd error.
 */
int container_monitor_addguest(containers_t *cs, container_config_t *cc)
{
	sd_event_source *pidfd_source = NULL;
	int ret = -1;
	int pidfd = -1;

	if (cs == NULL || cc == NULL) {
		return -1;
	}

	if (cc->runtime_stat.lxc == NULL) {
		return -1;
	}

	pidfd = cc->runtime_stat.lxc->init_pidfd(cc->runtime_stat.lxc);
	if (pidfd < 0) {
		return -2;
	}

	ret = sd_event_add_io(cs->event, &pidfd_source, pidfd, (EPOLLIN | EPOLLHUP | EPOLLERR), container_monitor_pidfd_handler, cs);
	if (ret < 0) {
		ret = -2;
		goto err_return;
	}

	cc->runtime_stat.pidfd_source = pidfd_source;

	return 0;

err_return:
	if (pidfd_source != NULL) {
		(void)sd_event_source_disable_unref(pidfd_source);
	}

	return -1;
}
/**
 *  Stop guest container monitoring using container monitor. (Reserve)
 *
 * @param [in]	cs	Pointer ro containers_t.
 * @param [in]	cc	Pointer to container_config_t that show which guest start monitoring.
 * @return int
 * @retval  0 Success.
 * @retval -1 Argument error. (Reserve)
 * @retval -2 pidfd error. (Reserve)
 */
int container_monitor_removeguest(containers_t *cs, container_config_t *cc)
{
	if (cs == NULL || cc == NULL) {
		return -1;
	}

	// No task in this case.

	return 0;
}

