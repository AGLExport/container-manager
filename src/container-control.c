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
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

#include "cm-utils.h"
#include "lxc-util.h"
#include "container-config.h"


/**
 * Event handler for server session socket
 *
 * @param [in]	event		Socket event source object
 * @param [in]	fd			File discriptor for socket session
 * @param [in]	revents		Active event (epooll)
 * @param [in]	userdata	Pointer to data_pool_service_handle
 * @return int	 0 success
 *				-1 internal error
 */
static int container_mngsm_state_machine(containers_t *cs, const uint8_t *buf)
{
	container_mngsm_command_header_t *phead;
	uint32_t command = 0;
	int ret = -1;

	phead = (container_mngsm_command_header_t*)buf;

	command = phead->command;

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr,"container_mngsm_state_machine: command %x\n", command);
	#endif

	switch(command) {
	case CONTAINER_MNGSM_COMMAND_DEVICEUPDATED :
		ret = container_device_updated(cs);

		break;
	case CONTAINER_MNGSM_COMMAND_NETIFUPDATED :
		ret = container_netif_updated(cs);

		break;
	case CONTAINER_MNGSM_COMMAND_GUEST_STATUS_CHANGE :
		{
			container_mngsm_guest_status_change_t *p = (container_mngsm_guest_status_change_t*)buf;

			ret = container_status_chage(cs, &p->data);
		}
		break;
	default:
		;
	}

	return 0;
}
/**
 * Event handler for server session socket
 *
 * @param [in]	event		Socket event source object
 * @param [in]	fd			File discriptor for socket session
 * @param [in]	revents		Active event (epooll)
 * @param [in]	userdata	Pointer to data_pool_service_handle
 * @return int	 0 success
 *				-1 internal error
 */
static int container_mngsm_commsocket_handler(sd_event_source *event, int fd, uint32_t revents, void *userdata)
{
	containers_t *cs = NULL;
	ssize_t rret = -1;
	int ret = -1;
	uint64_t buf[CONTAINER_MNGSM_COMMAND_BUFSIZEMAX/sizeof(uint64_t)];

	if (userdata == NULL) {
		//  Faile safe it unref.
		sd_event_source_disable_unref(event);
		return 0;
	}

	cs = (containers_t*)userdata;

	if ((revents & (EPOLLHUP | EPOLLERR)) != 0) {
		//  Faile safe it unref.
		sd_event_source_disable_unref(event);
	} else if ((revents & EPOLLIN) != 0) {
		// Event receive
		rret = read(fd, buf, sizeof(buf));
		if (rret > 0) {
			(void)container_mngsm_state_machine(cs, (const uint8_t*)buf);
		}

		return 0;
	}

	return -1;
}
/**
 * Sub function for create socket pair connectiong.
 *
 * @param [out]	cs	setup target for struct s_container.
 * @param [in]	event	Incetance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int container_mngsm_commsocket_setup(containers_t *cs, sd_event *event)
{
	sd_event_source *socket_source = NULL;
	struct s_container_mngsm *cms = NULL;
	int ret = -1;
	int pairfd[2] = {-1,-1};

	cms = (struct s_container_mngsm*)cs->cms;

	// Create state machine control socket
	ret = socketpair(AF_UNIX, SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK, AF_UNIX, pairfd); 
	if (ret < 0) {
		goto err_return;
	}

	//Primary fd use evelt loop
	ret = sd_event_add_io(event, &socket_source, pairfd[0], (EPOLLIN | EPOLLHUP | EPOLLERR), container_mngsm_commsocket_handler, cs);
	if (ret < 0) {
		ret = -1;
		goto err_return;
	}

	// Higher priority set.
	(void)sd_event_source_set_priority(socket_source, SD_EVENT_PRIORITY_NORMAL -10);

	// Set automatically fd closen at delete object.
	ret = sd_event_source_set_io_fd_own(socket_source, 1);
	if (ret < 0) {
		ret = -1;
		goto err_return;
	}

	cms->socket_source = socket_source;
	cms->primary_fd = pairfd[0];
	cms->secondary_fd = pairfd[1];

	return 0;

err_return:
	if (socket_source != NULL)
		(void)sd_event_source_disable_unref(socket_source);

	if (pairfd[1] != -1)
		close(pairfd[1]);

	if (pairfd[0] != -1)
		close(pairfd[0]);

	return -1;
}
/**
 * Regist device manager to container manager state machine
 *
 * @param [in]	cs	Incetance of containers_t
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
int container_mngsm_regist_device_manager(containers_t *cs, dynamic_device_manager_t *ddm)
{
	struct s_container_mngsm *cms = NULL;

	if (cs == NULL || ddm == NULL)
		return -2;

	cs->ddm = ddm;

	return 0;
}

/**
 * Container management state machine setup.
 *
 * @param [out]	pcs	return to containers_t*
 * @param [in]	event	Incetance of sd_event
 * @param [in]	config_dir	Path for container config dir.
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
int container_mngsm_setup(containers_t **pcs, sd_event *event, const char *config_file)
{
	containers_t *cs = NULL;
	struct s_container_mngsm *cms = NULL;
	int ret = -1;

	if (pcs == NULL || event == NULL)
		return -2;

	cs = create_container_configs(config_file);
	if (cs == NULL)
		return -1;

	cs->cms = (struct s_container_mngsm*)malloc(sizeof(struct s_container_mngsm));
	if (cs->cms == NULL)
		goto err_return;

	memset(cs->cms, 0, sizeof(struct s_container_mngsm));

	ret = container_mngsm_commsocket_setup(cs, event);
	if (ret < 0)
		goto err_return;

	cs->sys_state = CM_SYSTEM_STATE_RUN;
	cs->event = event;

	(*pcs) = cs;

	return 0;

err_return:

	if (cms != NULL) {
		(void)sd_event_source_disable_unref(cms->socket_source);
		free(cms);
	}

	if (cs != NULL)
		(void)release_container_configs(cs);

	return -1;
}

/**
 * Container management state machine cleanup.
 *
 * @param [in]	cs	Incetance of containers_t
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
int container_mngsm_cleanup(containers_t *cs)
{
	struct s_container_mngsm *cms = NULL;

	if (cs == NULL)
		return -2;

	container_mngsm_interface_free(cs);

	cms = cs->cms;
	if (cms != NULL) {
		(void)sd_event_source_disable_unref(cms->socket_source);
		free(cms);
	}

	(void)release_container_configs(cs);

	return 0;
}
