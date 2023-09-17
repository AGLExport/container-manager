/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-external-interface.c
 * @brief	This file include experimental external interface implementation of container manager.
 */
#include "container-manager-interface.h"
#include "container-external-interface.h"
#include "container-control-internal.h"
#include "container-workqueue.h"
#include "container.h"

#include "lxc-util.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <stdio.h>


/**
 * Convert from inner container status to external container status.
 *
 * @param [in]	status_inner	container status
 * @return int32_t
 * @retval -1>= status.
 * @retval -2 internal error.
 */
static int32_t container_external_interface_convert_status(int status_inner)
{
	int32_t ret = -2;

	switch(status_inner){
	case CONTAINER_DISABLE :
		ret = CONTAINER_EXTIF_GUEST_STATUS_DISABLE;
		break;
	case CONTAINER_NOT_STARTED :
		ret = CONTAINER_EXTIF_GUEST_STATUS_NOT_STARTED;
		break;
	case CONTAINER_STARTED :
		ret = CONTAINER_EXTIF_GUEST_STATUS_STARTED;
		break;
	case CONTAINER_REBOOT :
		ret = CONTAINER_EXTIF_GUEST_STATUS_REBOOT;
		break;
	case CONTAINER_SHUTDOWN :
		ret = CONTAINER_EXTIF_GUEST_STATUS_SHUTDOWN;
		break;
	case CONTAINER_DEAD :
		ret = CONTAINER_EXTIF_GUEST_STATUS_DEAD;
		break;
	case CONTAINER_EXIT :
		ret = CONTAINER_EXTIF_GUEST_STATUS_EXIT;
		break;
	default :
		break;
	}

	return ret;
}
/**
 * Command handler for "get-guest-info".
 *
 * @param [in]	cs			Pointer to containers_t
 * @param [out]	guests_info	Pointer to container_extif_command_get_response_t
 * @return int
 * @retval 0	Success to get information.
 * @retval -1	Internal error.(Reserve)
 * @retval -2	Argment error.
 */
static int container_external_interface_get_guest_info(containers_t *cs, container_extif_command_get_response_t *guests_info)
{
	int num_of_guest = 0;

	if ((cs == NULL) || (guests_info == NULL)) {
		return -2;
	}

	for (int i =0; i < cs->num_of_container; i++) {
		(void) strncpy(guests_info->guests[i].guest_name, cs->containers[i]->name, sizeof(guests_info->guests->guest_name));
		(void) strncpy(guests_info->guests[i].role_name, cs->containers[i]->role, sizeof(guests_info->guests->role_name));

		guests_info->guests[i].status = container_external_interface_convert_status(cs->containers[i]->runtime_stat.status);

		num_of_guest++;
	}

	guests_info->num_of_guests = num_of_guest;

	return 0;
}
/**
 * Command group handler for "get".
 *
 * @param [in]	pextif	Pointer to cm_external_interface_t
 * @param [in]	fd		File descriptor to use send response.
 * @param [in]	buf		Received data buffer
 * @param [in]	size	Received data size
 * @return int
 * @retval 0	Success to exec command.
 * @retval -1	Internal error.
 */
static int container_external_interface_command_get(cm_external_interface_t *pextif, int fd, void *buf, ssize_t size)
{
	container_extif_command_get_response_t guests_info;
	int ret = -1;
	ssize_t sret = -1;

	(void) memset(&guests_info, 0 , sizeof(guests_info));

	if(size >= (ssize_t)sizeof(container_extif_command_get_t)) {
		guests_info.header.command = CONTAINER_EXTIF_COMMAND_RESPONSE_GETGUESTS;
		ret = container_external_interface_get_guest_info(pextif->cs, &guests_info);
		if (ret == 0) {
			sret = write(fd, &guests_info, sizeof(guests_info));
			if (sret != (ssize_t)sizeof(guests_info)) {
				ret = -1;
			}
		} else {
			ret = -1;
		}
	} else {
		ret = -1;
	}

	return ret;
}
/**
 * Event handler for force reboot guest.
 *
 * @param [in]	cs		Pointer to containers_t
 * @param [in]	name	Name of guest container or guest role. It depend on argument role.
 * @param [in]	role	Argument name is guest name (=0) or role name (=1).
 * @return int
 * @retval 0	Success to force reboot request.
 * @retval -1	No selected name or role container.
 * @retval -2	Argument error.
 */
static int container_external_interface_force_reboot_guest(containers_t *cs, char *name, int role)
{
	int command_accept = -1;

	if ((cs == NULL) || (name == NULL)) {
		return -2;
	}

	for (int i =0; i < cs->num_of_container; i++) {
		container_config_t *cc = cs->containers[i];

		if (role == 0) {
			if (strncmp(cc->name, name, strlen(cc->name)) == 0) {
				(void) lxcutil_container_forcekill(cc);
				command_accept = 0;
			}
		} else {
			if (cc->runtime_stat.status == CONTAINER_STARTED) {
				if (strncmp(cc->role, name, strlen(cc->role)) == 0) {
					(void) lxcutil_container_forcekill(cc);
					command_accept = 0;
				}
			}
		}
	}

	return command_accept;
}
/**
 * Event handler for reboot guest.
 *
 * @param [in]	cs		Pointer to containers_t
 * @param [in]	name	Name of guest container or guest role. It depend on argument role.
 * @param [in]	role	Argument name is guest name (=0) or role name (=1).
 * @return int
 * @retval 0	Success to reboot request.
 * @retval -1	No selected name or role container.
 * @retval -2	Argument error.
 */
static int container_external_interface_reboot_guest(containers_t *cs, char *name, int role)
{
	int ret = -1;
	int command_accept = -1;

	if ((cs == NULL) || (name == NULL)) {
		return -2;
	}

	for (int i =0; i < cs->num_of_container; i++) {
		container_config_t *cc = cs->containers[i];

		if (role == 0) {
			if (strncmp(cc->name, name, strlen(cc->name)) == 0) {
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"container_external_interface_reboot_guest: reboot to %s, command req %s\n", cc->name, name);
				#endif
				ret = container_request_reboot(cc, cs->sys_state);
				if (ret == 0) {
					command_accept = 0;
				}
			}
		} else {
			if (cc->runtime_stat.status == CONTAINER_STARTED) {
				if (strncmp(cc->role, name, strlen(cc->role)) == 0) {
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"container_external_interface_reboot_guest: reboot to %s, command req %s\n", cc->name, name);
					#endif
					ret = container_request_reboot(cc, cs->sys_state);
					if (ret == 0) {
						command_accept = 0;
					}
				}
			}
		}
	}

	return command_accept;
}
/**
 * Event handler for shutdown guest.
 *
 * @param [in]	cs		Pointer to containers_t
 * @param [in]	name	Name of guest container or guest role. It depend on argument role.
 * @param [in]	role	Argument name is guest name (=0) or role name (=1).
 * @return int
 * @retval 0	Success to reboot request.
 * @retval -1	No selected name or role container.
 * @retval -2	Argument error.
 */
static int container_external_interface_shutdown_guest(containers_t *cs, char *name, int role)
{
	int ret = -1;
	int command_accept = -1;

	if ((cs == NULL) || (name == NULL)) {
		return -2;
	}

	for (int i =0; i < cs->num_of_container; i++) {
		container_config_t *cc = cs->containers[i];

		if (role == 0) {
			if (strncmp(cc->name, name, strlen(cc->name)) == 0) {
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"container_external_interface_shutdown_guest: shutdown to %s, command req %s\n", cc->name, name);
				#endif
				ret = container_request_shutdown(cc, cs->sys_state);
				if (ret == 0) {
					command_accept = 0;
				}
			}
		} else {
			if (cc->runtime_stat.status == CONTAINER_STARTED) {
				if (strncmp(cc->role, name, strlen(cc->role)) == 0) {
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"container_external_interface_shutdown_guest: shutdown to %s, command req %s\n", cc->name, name);
					#endif
					ret = container_request_shutdown(cc, cs->sys_state);
					if (ret == 0) {
						command_accept = 0;
					}
				}
			}
		}
	}

	return command_accept;
}
/**
 * Command group handler for "lifecycle".
 *
 * @param [in]	pextif	Pointer to cm_external_interface_t
 * @param [in]	fd		File descriptor to use send response.
 * @param [in]	buf		Received data buffer
 * @param [in]	size	Received data size
 * @param [in]	role	Argument name is guest name (=0) or role name (=1).
 * @return int
 * @retval 0	Success to exec command.
 * @retval -1	Internal error.
 */
static int container_external_interface_command_lifecycle(cm_external_interface_t *pextif, int fd, void *buf, ssize_t size, int role)
{
	container_extif_command_lifecycle_t *pcom_life = (container_extif_command_lifecycle_t*)buf;
	container_extif_command_lifecycle_response_t response;
	ssize_t sret = -1;
	int ret = -1;

	(void) memset(&response, 0 , sizeof(response));

	if(size >= (ssize_t)sizeof(container_extif_command_lifecycle_t)) {
		if (pcom_life->subcommand == CONTAINER_EXTIF_SUBCOMMAND_FORCEREBOOT_GUEST) {
			// Test imp. TODO change state machine request
			ret = container_external_interface_force_reboot_guest(pextif->cs, pcom_life->guest_name , role);
			if (ret != 0) {
				response.response = CONTAINER_EXTIF_LIFECYCLE_RESPONSE_NONAME;
			} else {
				response.response = CONTAINER_EXTIF_LIFECYCLE_RESPONSE_ACCEPT;
			}
		} else if (pcom_life->subcommand == CONTAINER_EXTIF_SUBCOMMAND_REBOOT_GUEST) {
			// Test imp. TODO change state machine request
			ret = container_external_interface_reboot_guest(pextif->cs, pcom_life->guest_name , role);
			if (ret != 0) {
				response.response = CONTAINER_EXTIF_LIFECYCLE_RESPONSE_NONAME;
			} else {
				response.response = CONTAINER_EXTIF_LIFECYCLE_RESPONSE_ACCEPT;
			}
		} else if (pcom_life->subcommand == CONTAINER_EXTIF_SUBCOMMAND_SHUTDOWN_GUEST) {
			// Test imp. TODO change state machine request
			ret = container_external_interface_shutdown_guest(pextif->cs, pcom_life->guest_name , role);
			if (ret != 0) {
				response.response = CONTAINER_EXTIF_LIFECYCLE_RESPONSE_NONAME;
			} else {
				response.response = CONTAINER_EXTIF_LIFECYCLE_RESPONSE_ACCEPT;
			}
		} else {
			// other is not support
			response.response = CONTAINER_EXTIF_LIFECYCLE_RESPONSE_ERROR;
		}

		if (ret == 0) {
			sret = write(fd, &response, sizeof(response));
			if (sret != (ssize_t)sizeof(response)) {
				ret = -1;
			}
		} else {
			ret = -1;
		}
	} else {
		ret = -1;
	}

	return ret;
}
/**
 * Command group handler for "change".
 *
 * @param [in]	pextif	Pointer to cm_external_interface_t
 * @param [in]	fd		File descriptor to use send response.
 * @param [in]	buf		Received data buffer
 * @param [in]	size	Received data size
 * @return int
 * @retval 0	Success to exec command.
 * @retval -1	Internal error.
 */
static int container_external_interface_command_change(cm_external_interface_t *pextif, int fd, void *buf, ssize_t size)
{
	container_extif_command_change_t *pcom_change = (container_extif_command_change_t*)buf;
	container_extif_command_change_response_t response;
	ssize_t sret = -1;
	int ret = -1;

	(void) memset(&response, 0 , sizeof(response));
	response.header.command = CONTAINER_EXTIF_COMMAND_RESPONSE_CHANGE;
	response.response = CONTAINER_EXTIF_CHANGE_RESPONSE_ERROR;

	if(size >= (ssize_t)sizeof(container_extif_command_change_t)) {
		containers_t *cs = pextif->cs;
		char *role = NULL;

		for (int i =0; i < cs->num_of_container; i++) {
			if (strcmp(cs->containers[i]->name, pcom_change->guest_name) == 0) {
				role = cs->containers[i]->role;
				break;
			}
		}

		if (role != NULL) {
			container_manager_role_config_t *cmrc = NULL;
			dl_list_for_each(cmrc, &cs->cmcfg->role_list, container_manager_role_config_t, list) {
				if (cmrc->name != NULL) {
					if (strcmp(cmrc->name, role) == 0) {
						container_manager_role_elem_t *pelem = NULL;

						pelem = dl_list_first(&cmrc->container_list, container_manager_role_elem_t, list) ;
						if (pelem != NULL && pelem->cc != NULL) {
							// latest active guest move to disable
							dl_list_del(&pelem->list);
							dl_list_add_tail(&cmrc->container_list, &pelem->list);

							{
								container_manager_role_elem_t *pelem2 = NULL;

								dl_list_for_each(pelem2, &cmrc->container_list, container_manager_role_elem_t, list) {
									if (pelem2->cc != NULL) {
										if (strcmp(pelem2->cc->name, pcom_change->guest_name) == 0) {
											// latest active guest move to disable
											dl_list_del(&pelem2->list);
											dl_list_add(&cmrc->container_list, &pelem2->list);

											response.response = CONTAINER_EXTIF_CHANGE_RESPONSE_ACCEPT;
											break;
										}
									}
								}
							}
						}
						break;
					}
				}
			}
		} else {
			response.response = CONTAINER_EXTIF_CHANGE_RESPONSE_NONAME;
		}

		sret = write(fd, &response, sizeof(response));
		if (sret != (ssize_t)sizeof(response)) {
			ret = -1;
		}

	} else {
		ret = -1;
	}

	return ret;
}
/**
 * Command group handler for "test".
 *
 * @param [in]	pextif	Pointer to cm_external_interface_t
 * @param [in]	fd		File descriptor to use send response.
 * @param [in]	buf		Received data buffer
 * @param [in]	size	Received data size
 * @param [in]	role	Argument name is guest name (=0) or role name (=1).
 * @return int
 * @retval 0	Success to exec command.
 * @retval -1	Internal error.
 */
static const char *cstr_option_device = "device=/dev/mmcblk1p7";

static int container_external_interface_command_test(cm_external_interface_t *pextif, int fd, void *buf, ssize_t size)
{
	container_extif_command_test_trigger_t *pcom_test = (container_extif_command_test_trigger_t*)buf;
	container_extif_command_test_trigger_response_t response;
	ssize_t sret = -1;
	int ret = -1;

	(void) memset(&response, 0 , sizeof(response));

	if(size >= (ssize_t)sizeof(container_extif_command_test_trigger_t)) {
		if (pcom_test->code == 0) {
			containers_t *cs = pextif->cs;
			int target = -1;
			for (int i =0; i < cs->num_of_container; i++) {
				if (strcmp(cs->containers[i]->name, "agl-momi-ivi-demo") == 0) {
					target = i;
					break;
				}
			}

			// Container workqueue test
			if (target > 0) {
				ret = container_workqueue_schedule(&(cs->containers[target]->workqueue), "fsck", cstr_option_device, 1);
				if (ret == 0) {
					response.response = 0;
				} else {
					response.response = -1;
				}
			}
		} else if (pcom_test->code == 1) {
			containers_t *cs = pextif->cs;
			int target = -1;
			for (int i =0; i < cs->num_of_container; i++) {
				if (strcmp(cs->containers[i]->name, "agl-momi-ivi-demo") == 0) {
					target = i;
					break;
				}
			}

			// Container workqueue test
			if (target > 0) {
				ret = container_workqueue_schedule(&(cs->containers[target]->workqueue), "erase", cstr_option_device, 1);
				if (ret == 0) {
					response.response = 0;
				} else {
					response.response = -1;
				}
			}
		} else {
			// other is not support
			response.response = -1;
		}

		if (ret == 0) {
			sret = write(fd, &response, sizeof(response));
			if (sret != (ssize_t)sizeof(response)) {
				ret = -1;
			}
		} else {
			ret = -1;
		}
	} else {
		ret = -1;
	}

	return ret;
}
/**
 * Event handler for external interface session socket
 *
 * @param [in]	pextif	Pointer to cm_external_interface_t
 * @param [in]	fd		File descriptor to use send response.
 * @param [in]	buf		Received data buffer
 * @param [in]	size	Received data size
 * @return int
 * @retval 0 success (need disconnect session).
 * @retval -1 internal error.
 */
static int container_external_interface_exec(cm_external_interface_t *pextif, int fd, void *buf, ssize_t size)
{
	container_extif_command_header_t *pheader = NULL;
	int ret = 0;

	if ((buf == NULL) || (size < (ssize_t)sizeof(container_extif_command_header_t))) {
		return -1;
	}

	pheader = (container_extif_command_header_t*)buf;

	switch (pheader->command) {
	case CONTAINER_EXTIF_COMMAND_GETGUESTS :
		ret = container_external_interface_command_get(pextif, fd, buf, size);
		break;
	case CONTAINER_EXTIF_COMMAND_LIFECYCLE_GUEST_NAME :
		ret = container_external_interface_command_lifecycle(pextif, fd, buf, size, 0);
		break;
	case CONTAINER_EXTIF_COMMAND_LIFECYCLE_GUEST_ROLE :
		ret = container_external_interface_command_lifecycle(pextif, fd, buf, size, 1);
		break;
	case CONTAINER_EXTIF_COMMAND_CHANGE_ACTIVE_GUEST_NAME :
		ret = container_external_interface_command_change(pextif, fd, buf, size);
		break;
	case CONTAINER_EXTIF_COMMAND_TEST_TRIGGER :
		ret = container_external_interface_command_test(pextif, fd, buf, size);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}
/**
 * Event handler for external interface session socket.
 *
 * @param [in]	event		Socket event source object
 * @param [in]	fd			File descriptor for socket session
 * @param [in]	revents		Active event (epoll)
 * @param [in]	userdata	Pointer to data_pool_service_handle
 * @return int
 * @retval 0 success
 * @retval -1 internal error.
 */
static int container_external_interface_sessions_handler(sd_event_source *event, int fd, uint32_t revents, void *userdata)
{
	cm_external_interface_t *pextif = (cm_external_interface_t*)userdata;
	uint64_t buf[CONTAINER_EXTIF_COMMAND_BUFSIZEMAX/sizeof(uint64_t)];
	ssize_t sret = -1;

	if ((revents & (EPOLLHUP | EPOLLERR)) != 0) {
		// Disconnect session
		(void) sd_event_source_disable_unref(pextif->interface_session_evsource);
		pextif->interface_session_evsource = NULL;
	} else if ((revents & EPOLLIN) != 0) {
		// Receive
		sret = read(fd, buf, sizeof(buf));
		if (sret > 0) {
			(void) container_external_interface_exec(pextif, fd, buf, sret);
		}
		// close session
		(void) sd_event_source_disable_unref(pextif->interface_session_evsource);
		pextif->interface_session_evsource = NULL;
	} else {
		;	//nop
	}

	return 0;
}

/**
 * Event handler for server socket to use incoming event
 *
 * @param [in]	event		Socket event source object
 * @param [in]	fd			File descriptor for socket session
 * @param [in]	revents		Active event (epoll)
 * @param [in]	userdata	Pointer to data_pool_service_handle
 * @return int
 * @retval 0 success
 * @retval -1 internal error.
 */
static int container_external_interface_incoming_handler(sd_event_source *event, int fd, uint32_t revents, void *userdata)
{
	cm_external_interface_t *pextif = (cm_external_interface_t*)userdata;
	int sessionfd = -1;
	int ret = -1;

	if ((revents & (EPOLLHUP | EPOLLERR)) != 0) {
		// False safe: Disable server socket
		if (pextif != NULL) {
			pextif->interface_evsource = sd_event_source_disable_unref(pextif->interface_evsource);
		}

		goto error_return;

	} else if ((revents & EPOLLIN) != 0) {
		// New session
		do {
			sessionfd = accept4(fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
		} while ((sessionfd < 0) && (errno == EINTR));

		if (sessionfd < 0) {
			goto error_return;
		}

		if (pextif == NULL) {
			goto error_return;
		}

		if(pextif->interface_session_evsource != NULL) {
			// external interface is one session only
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"container_external_interface_incoming_handler: double session\n");
			#endif
			goto error_return;
		}

		ret = sd_event_add_io(pextif->parent_eventloop, &pextif->interface_session_evsource, sessionfd,
								(EPOLLIN | EPOLLHUP | EPOLLERR), container_external_interface_sessions_handler, pextif);
		if (ret < 0) {
			goto error_return_b;
		}

		// Set automatically fd close at delete object.
		ret = sd_event_source_set_io_fd_own(pextif->interface_session_evsource, 1);
		if (ret < 0) {
			goto error_return_b;
		}
		// After this, shall not close sessionfd by close.
		sessionfd = -1;
	} else {
		goto error_return;
	}

	return 0;

error_return_b:
	if ((pextif != NULL) && (pextif->interface_session_evsource != NULL)) {
		(void) sd_event_source_disable_unref(pextif->interface_session_evsource);
		pextif->interface_session_evsource = NULL;
	}

error_return:
	if (sessionfd >= 0) {
		(void) close(sessionfd);
	}

	return 0;
}
/**
 * Setup to container manager external interface.
 *
 * @param [in]	cs		Pointer to containers_t.
 * @param [in]	event	sd event loop handle
 * @return int
 * @retval 0	Success to setup container manager external interface.
 * @retval -1	Internal error.
 * @retval -2	Argument error.
 */
int container_external_interface_setup(containers_t *cs, sd_event *event)
{
	sd_event_source *socket_source = NULL;
	struct sockaddr_un name;
	cm_external_interface_t *pextif = NULL;
	struct s_container_mngsm *cms = NULL;
	int fd = -1;
	int ret = -1;

	if ((cs == NULL) || (event == NULL)) {
		return -2;
	}

	cms = cs->cms;
	if (cms == NULL) {
		return -2;
	}

	pextif = (cm_external_interface_t*) malloc(sizeof(cm_external_interface_t));
	if (pextif == NULL) {
		ret = -1;
		goto err_return;
	}

	(void) memset(pextif, 0, sizeof(*pextif));

	pextif->parent_eventloop = event;

	// Create server socket.
	fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, AF_UNIX);
	if (fd < 0) {
		ret = -1;
		goto err_return;
	}

	(void) memset(&name, 0, sizeof(name));
	name.sun_family = AF_UNIX;
	(void) memcpy(name.sun_path, CONTAINER_MANAGER_EXTERNAL_SOCKET_NAME, sizeof(CONTAINER_MANAGER_EXTERNAL_SOCKET_NAME));

	ret = bind(fd, (const struct sockaddr *) &name, sizeof(CONTAINER_MANAGER_EXTERNAL_SOCKET_NAME) + sizeof(sa_family_t));
	if (ret < 0) {
		ret = -1;
		goto err_return;
	}

	// Single session only to container manager external interface
	ret = listen(fd, 1);
	if (ret < 0) {
		ret = -1;
		goto err_return;
	}

	ret = sd_event_add_io(event, &socket_source, fd, EPOLLIN, container_external_interface_incoming_handler, pextif);
	if (ret < 0) {
		ret = -1;
		goto err_return;
	}

	// Set automatically fd close at delete object.
	ret = sd_event_source_set_io_fd_own(socket_source, 1);
	if (ret < 0) {
		ret = -1;
		goto err_return;
	}

	// After the automatic fd close setting shall not close fd in error path
	fd = -1;

	pextif->interface_evsource = socket_source;
	pextif->cs = cs;

	cms->cm_ext_if = pextif;

	return 0;

err_return:
	(void) sd_event_source_disable_unref(socket_source);
	(void) free(pextif);
	if (fd != -1) {
		(void) close(fd);
	}

	return ret;
}

/**
 * Cleanup container manager external interface.
 *
 * @param [in]	cs		Pointer to containers_t.
 * @retval 0	Success to cleanup container manager external interface.
 * @retval -1	Internal error.
 * @retval -2	Argument error.
 */
int container_external_interface_cleanup(containers_t *cs)
{
	container_mngsm_t *cms = NULL;
	cm_external_interface_t *pextif = NULL;

	if (cs == NULL) {
		return -2;
	}

	cms = (container_mngsm_t*)cs->cms;
	if (cms == NULL) {
		return -2;
	}

	pextif = cms->cm_ext_if;
	if (pextif == NULL) {
		return -2;
	}

	if (pextif->interface_session_evsource != NULL) {
		(void) sd_event_source_disable_unref(pextif->interface_session_evsource);
	}

	(void) sd_event_source_disable_unref(pextif->interface_evsource);
	(void) free(pextif);

	return 0;
}
