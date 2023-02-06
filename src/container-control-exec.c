/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-control-exec.c
 * @brief	This file include implementation for container manager event operations.
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
#include <linux/magic.h>

#include "cm-utils.h"
#include "lxc-util.h"
#include "container-config.h"
#include "device-control.h"

static int container_start_preprocess_base(container_baseconfig_t *bc);
static int container_cleanup_preprocess_base(container_baseconfig_t *bc);
static int container_get_active_guest_by_role(containers_t *cs, char *role, container_config_t **active_cc);

int container_restart(container_config_t *cc);

/**
 * The function for dynamic network interface add (remove) event handling.
 *
 * @param [in]	cc	Pointer to container_config_t.
 * @param [in]	ddm	Pointer to dynamic_device_manager_t.
 * @return int
 * @retval  0 Success to operation.
 * @retval -1 Critical error.(Reserve)
 */
int container_netif_update_guest(container_config_t *cc, dynamic_device_manager_t *ddm)
{
	int ret = -1;
	network_interface_manager_t *netif = NULL;
	container_dynamic_netif_t *cdn = NULL;
	network_interface_info_t *nii = NULL;
	container_dynamic_netif_elem_t *cdne = NULL;

	if (cc->runtime_stat.status != CONTAINER_STARTED) {
		// Not running container, pending
		return 0;
	}

	ret = network_interface_info_get(&netif, ddm);
	if (ret < 0) {
		// Not starting container, pending
		return 0;
	}

	cdn = &cc->netifconfig.dynamic_netif;

	// status clean
	dl_list_for_each(cdne, &cdn->dynamic_netiflist, container_dynamic_netif_elem_t, list) {
		cdne->is_available = 0;
	}

	// check new netif
	dl_list_for_each(nii, &netif->nllist, network_interface_info_t, list) {

		if (nii->ifindex <= 0)
			continue;

		dl_list_for_each(cdne, &cdn->dynamic_netiflist, container_dynamic_netif_elem_t, list) {
			if (cdne->ifindex == 0 && strncmp(nii->ifname, cdne->ifname, sizeof(nii->ifname)) == 0) {
				// found new interface for own
				cdne->ifindex = nii->ifindex;
				cdne->is_available = 1;

				//add net interface to guest container
				ret = lxcutil_dynamic_networkif_add_to_guest(cc, cdne);
				if (ret < 0) {
					// fail back
					cdne->ifindex = 0;
					cdne->is_available = 0;
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr, "[fail] network if update add %s to %s\n", cdne->ifname, cc->name);
					#endif
				} else {
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr, "network if update add %s to %s\n", cdne->ifname, cc->name);
					#endif
					;
				}
			} else if (cdne->ifindex == nii->ifindex) {
				// existing netif
				cdne->is_available = 1;
			}
		}
	}

	// disable removed netif
	dl_list_for_each(cdne, &cdn->dynamic_netiflist, container_dynamic_netif_elem_t, list) {
		if (cdne->is_available == 0 && cdne->ifindex != 0) {
			cdne->ifindex = 0;

			// Don't need memory free.
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr, "network if update removed %s from %s\n", cdne->ifname, cc->name);
			#endif
		}
	}

	return 0;
}
/**
 * The function for dynamic network interface cleanup for stopped (exited, dead) guest container.
 *
 * @param [in]	cc	Pointer to container_config_t.
 * @return int
 * @retval  0 Success to operation.
 * @retval -1 Critical error.(Reserve)
 */
int container_netif_remove_element(container_config_t *cc)
{
	container_dynamic_netif_elem_t *cdne = NULL;
	container_dynamic_netif_t *cdn = NULL;

	cdn = &cc->netifconfig.dynamic_netif;
	// netif remove
	dl_list_for_each(cdne, &cdn->dynamic_netiflist, container_dynamic_netif_elem_t, list) {
		cdne->ifindex = 0;

		// Don't need memory free.
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "network if update removed %s from %s\n", cdne->ifname, cc->name);
		#endif
	}

	return 0;
}
/**
 * Dispatch dynamic network interface update operation to all guest container.
 *
 * @param [in]	cs	Pointer to containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
int container_netif_updated(containers_t *cs)
{
	int num = 0;
	int ret = 1;
	int result = -1;
	container_config_t *cc = NULL;

	num = cs->num_of_container;

	for(int i=0;i < num;i++) {
		cc = cs->containers[i];
		ret = container_netif_update_guest(cc, cs->ddm);
		if (ret < 0)
			goto err_ret;
	}

	return 0;

err_ret:

	return result;
}
/**
 * Container status change event handler in container exit.
 * This handler is judging next container status using system state, current status and exit event.
 *
 * @param [in]	cs		Pointer to containers_t
 * @param [in]	data	Pointer to container_mngsm_guest_exit_data_t, it's include detail of exit event.
 * @return int
 * @retval  0 Success to change next state.
 * @retval -1 Got undefined state.
 */
int container_exited(containers_t *cs, container_mngsm_guest_exit_data_t *data)
{
	int num = 0, container_num = 0;
	int result = 0;
	container_config_t *cc = NULL;

	num = cs->num_of_container;
	container_num = data->container_number;
	if (container_num < 0 || num <= container_num)
		return -1;

	cc = cs->containers[container_num];

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr,"container_exited : %s\n", cc->name);
	#endif

	if (cs->sys_state  == CM_SYSTEM_STATE_RUN) {
		if (cc->runtime_stat.status == CONTAINER_NOT_STARTED) {
			// not running, not need shutdown
			;
		} else if (cc->runtime_stat.status == CONTAINER_STARTED) {
			// now running, guest was dead
			cc->runtime_stat.status = CONTAINER_DEAD;

			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			fprintf(stderr,"[CM CRITICAL ERROR] container %s was dead.\n", cc->name);
			#endif
		} else if (cc->runtime_stat.status == CONTAINER_REBOOT) {
			// Current status is reboot, guest status change to dead
			cc->runtime_stat.status = CONTAINER_DEAD;
		} else if (cc->runtime_stat.status == CONTAINER_SHUTDOWN) {
			// Already requested shutdown, change to not start state.
			cc->runtime_stat.status = CONTAINER_NOT_STARTED;
		} else if (cc->runtime_stat.status == CONTAINER_DEAD) {
			// Already dead container, no update status.
			;
		} else if (cc->runtime_stat.status == CONTAINER_EXIT) {
			// undefined state
			result = -1;
		} else {
			// undefined state
			result = -1;
		}
	} else if (cs->sys_state  == CM_SYSTEM_STATE_SHUTDOWN) {
		if (cc->runtime_stat.status == CONTAINER_NOT_STARTED) {
			// May not get this state, change to exit state. (fail safe)
			cc->runtime_stat.status = CONTAINER_EXIT;
		} else if (cc->runtime_stat.status == CONTAINER_STARTED) {
			// cross event between crash and system shutdown, change to exit state.
			cc->runtime_stat.status = CONTAINER_EXIT;
		} else if (cc->runtime_stat.status == CONTAINER_REBOOT) {
			// exit container after system shutdown request.
			cc->runtime_stat.status = CONTAINER_EXIT;
		} else if (cc->runtime_stat.status == CONTAINER_SHUTDOWN) {
			// exit container after system shutdown request.
			cc->runtime_stat.status = CONTAINER_EXIT;
		} else if (cc->runtime_stat.status == CONTAINER_DEAD) {
			// May not get this state, change to exit state. (fail safe)
			cc->runtime_stat.status = CONTAINER_EXIT;
		} else if (cc->runtime_stat.status == CONTAINER_EXIT) {
			// Already exit, not need new action
			;
		} else {
			// undefined state
			result = -1;
		}
	} else {
		// undefined state
		result = -1;
	}

	(void) container_terminate(cc);

	return result;
}
/**
 * Container status change event handler in requested container shutdown.
 * This handler is judging next container status using system state, current status and shutdown request event.
 * When status change from CONTAINER_STARTED, this function send container shutdown request to guest container.
 *
 * @param [in]	cc			Pointer to container_config_t
 * @param [in]	sys_state	Current system state. ( CM_SYSTEM_STATE_RUN or CM_SYSTEM_STATE_SHUTDOWN )
 * @return int
 * @retval  0 Success to change next state.
 * @retval -1 Got undefined state.
 */
int container_request_shutdown(container_config_t *cc, int sys_state)
{
	int ret = -1;
	int result = 0;

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr,"container_request_shutdown to %s (%d)\n", cc->name, sys_state);
	#endif

	if (sys_state == CM_SYSTEM_STATE_RUN) {
		if (cc->runtime_stat.status == CONTAINER_NOT_STARTED) {
			// not ruining, not need shutdown
			;
		} else if (cc->runtime_stat.status == CONTAINER_STARTED) {
			// now ruining, send shutdown request
			ret = lxcutil_container_shutdown(cc);
			if (ret < 0) {
				//In fail case, force kill.
				(void) lxcutil_container_forcekill(cc);
				(void) container_cleanup(cc);
				cc->runtime_stat.status = CONTAINER_NOT_STARTED; // guest is force dead
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				fprintf(stderr,"[CM CRITICAL ERROR] container_request_shutdown fourcekill to %s.\n", cc->name);
				#endif
			} else {
				int64_t timeout = 0;

				// Set timeout
				timeout = get_current_time_ms();
				if (timeout < 0)
					timeout = 0;
				timeout = timeout + cc->baseconfig.lifecycle.timeout;
				cc->runtime_stat.timeout = timeout;

				//Requested, wait to exit.
				cc->runtime_stat.status = CONTAINER_SHUTDOWN;
			}
		} else if (cc->runtime_stat.status == CONTAINER_REBOOT) {
			// When reboot state, state not need to change.
			;
		} else if (cc->runtime_stat.status == CONTAINER_SHUTDOWN) {
			// Already requested shutdown, not need new action.
			;
		} else if (cc->runtime_stat.status == CONTAINER_DEAD) {
			// Already dead container, disable to re-launch.
			cc->runtime_stat.status = CONTAINER_NOT_STARTED;
		} else if (cc->runtime_stat.status == CONTAINER_EXIT) {
			// undefined state
			result = -1;
		} else if (cc->runtime_stat.status == CONTAINER_DISABLE) {
			// disabled container, not need new action
			;
		} else {
			// undefined state
			result = -1;
		}
	} else if (sys_state == CM_SYSTEM_STATE_SHUTDOWN) {
		if (cc->runtime_stat.status == CONTAINER_NOT_STARTED) {
			// not running, change to exit state
			cc->runtime_stat.status = CONTAINER_EXIT;
		} else if (cc->runtime_stat.status == CONTAINER_STARTED) {
			// now running, send shutdown request

			ret = lxcutil_container_shutdown(cc);
			if (ret < 0) {
				//In fail case, force kill.
				(void) lxcutil_container_forcekill(cc);
				(void) container_cleanup(cc);
				cc->runtime_stat.status = CONTAINER_EXIT; // guest is force exit
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				fprintf(stderr,"[CM CRITICAL ERROR] container_request_shutdown fourcekill to %s.\n", cc->name);
				#endif
			} else {
				int64_t timeout = 0;

				// Set timeout
				timeout = get_current_time_ms();
				if (timeout < 0)
					timeout = 0;
				timeout = timeout + cc->baseconfig.lifecycle.timeout;
				cc->runtime_stat.timeout = timeout;

				//Requested, wait to exit.
				cc->runtime_stat.status = CONTAINER_SHUTDOWN;
			}
		} else if (cc->runtime_stat.status == CONTAINER_REBOOT) {
			// Already requested reboot, change to shutdown state.
			cc->runtime_stat.status = CONTAINER_SHUTDOWN;
		} else if (cc->runtime_stat.status == CONTAINER_SHUTDOWN) {
			// Already requested shutdown, not need new action.
			;
		} else if (cc->runtime_stat.status == CONTAINER_DEAD) {
			// Already dead container, it's already exit.
			cc->runtime_stat.status = CONTAINER_EXIT;
		} else if (cc->runtime_stat.status == CONTAINER_EXIT) {
			// Already exit, not need new action
			;
		} else if (cc->runtime_stat.status == CONTAINER_DISABLE) {
			// disabled container, not need new action
			;
		} else {
			// undefined state
			result = -1;
		}
	} else {
		// undefined state
		result = -1;
	}

	return result;
}
/**
 * Container status change event handler in requested container reboot.
 * This handler is judging next container status using system state, current status and reboot request event.
 * When status change from CONTAINER_STARTED, this function send container shutdown request to guest container.
 *
 * @param [in]	cc			Pointer to container_config_t
 * @param [in]	sys_state	Current system state. ( CM_SYSTEM_STATE_RUN or CM_SYSTEM_STATE_SHUTDOWN )
 * @return int
 * @retval  0 Success to change next state.
 * @retval -1 Got undefined state.
 */
int container_request_reboot(container_config_t *cc, int sys_state)
{
	int ret = -1;
	int result = 0;

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr,"container_request_reboot to %s (%d)\n", cc->name, sys_state);
	#endif

	if (sys_state == CM_SYSTEM_STATE_RUN) {
		if (cc->runtime_stat.status == CONTAINER_NOT_STARTED) {
			// not ruining, not need reboot
			;
		} else if (cc->runtime_stat.status == CONTAINER_STARTED) {
			// now ruining, send reboot(shutdown) request
			ret = lxcutil_container_shutdown(cc);
			if (ret < 0) {
				//In fail case, force kill.
				(void) lxcutil_container_forcekill(cc);
				(void) container_cleanup(cc);
				cc->runtime_stat.status = CONTAINER_DEAD; // guest is force dead
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				fprintf(stderr,"[CM CRITICAL ERROR] container_request_reboot fourcekill to %s.\n", cc->name);
				#endif
			} else {
				int64_t timeout = 0;

				// Set timeout
				timeout = get_current_time_ms();
				if (timeout < 0)
					timeout = 0;
				timeout = timeout + cc->baseconfig.lifecycle.timeout;
				cc->runtime_stat.timeout = timeout;

				//Requested, wait to exit.
				cc->runtime_stat.status = CONTAINER_REBOOT;
			}
		} else if (cc->runtime_stat.status == CONTAINER_REBOOT) {
			// When reboot state, state not need to change.
			;
		} else if (cc->runtime_stat.status == CONTAINER_SHUTDOWN) {
			// Already requested shutdown, not need new action.
			;
		} else if (cc->runtime_stat.status == CONTAINER_DEAD) {
			// Already dead container, disable to re-launch.
			cc->runtime_stat.status = CONTAINER_NOT_STARTED;
		} else if (cc->runtime_stat.status == CONTAINER_EXIT) {
			// undefined state
			result = -1;
		} else if (cc->runtime_stat.status == CONTAINER_DISABLE) {
			// disabled container, not need new action
			;
		} else {
			// undefined state
			result = -1;
		}
	} else if (sys_state == CM_SYSTEM_STATE_SHUTDOWN) {
		if (cc->runtime_stat.status == CONTAINER_NOT_STARTED) {
			// not running, change to exit state
			cc->runtime_stat.status = CONTAINER_EXIT;
		} else if (cc->runtime_stat.status == CONTAINER_STARTED) {
			// now running, send shutdown request

			ret = lxcutil_container_shutdown(cc);
			if (ret < 0) {
				//In fail case, force kill.
				(void) lxcutil_container_forcekill(cc);
				(void) container_cleanup(cc);
				cc->runtime_stat.status = CONTAINER_EXIT; // guest is force exit
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				fprintf(stderr,"[CM CRITICAL ERROR] container_request_shutdown fourcekill to %s.\n", cc->name);
				#endif
			} else {
				int64_t timeout = 0;

				// Set timeout
				timeout = get_current_time_ms();
				if (timeout < 0)
					timeout = 0;
				timeout = timeout + cc->baseconfig.lifecycle.timeout;
				cc->runtime_stat.timeout = timeout;

				//Requested, wait to exit.
				cc->runtime_stat.status = CONTAINER_SHUTDOWN;
			}
		} else if (cc->runtime_stat.status == CONTAINER_REBOOT) {
			// Already requested reboot, change to shutdown state.
			cc->runtime_stat.status = CONTAINER_SHUTDOWN;
		} else if (cc->runtime_stat.status == CONTAINER_SHUTDOWN) {
			// Already requested shutdown, not need new action.
			;
		} else if (cc->runtime_stat.status == CONTAINER_DEAD) {
			// Already dead container, it's already exit.
			cc->runtime_stat.status = CONTAINER_EXIT;
		} else if (cc->runtime_stat.status == CONTAINER_EXIT) {
			// Already exit, not need new action
			;
		} else if (cc->runtime_stat.status == CONTAINER_DISABLE) {
			// disabled container, not need new action
			;
		} else {
			// undefined state
			result = -1;
		}
	} else {
		// undefined state
		result = -1;
	}

	return result;
}
/**
 * Container shutdown event handler.
 * This handler handle shutdown event that receive from system management daemon (typically init).
 * This function set CM_SYSTEM_STATE_SHUTDOWN to system state of container manager.
 *
 * @param [in]	cs		Pointer to containers_t
 * @return int
 * @retval  0 Success to change next state.
 * @retval -1 Failed to request shutdown in one or more guest.
 */
int container_manager_shutdown(containers_t *cs)
{
	int num = 0;
	int fail_count = 0;
	int ret = -1;
	container_config_t *cc = NULL;

	cs->sys_state = CM_SYSTEM_STATE_SHUTDOWN; // change to shutdown state

	// Send shutdown request to each container
	num = cs->num_of_container;

	for(int i=0;i < num;i++) {
		cc = cs->containers[i];

		ret = container_request_shutdown(cc, cs->sys_state);
		if (ret < 0) {
			fail_count++;
		}
	}

	if (fail_count > 0)
		return -1;

	return 0;
}
/**
 * Cyclic event handler for container manager state machine.
 * This handler handle to;
 *  Launch retry in dead state.
 *  Exchange active guest and launch after exit old active guest.
 *  Timeout test for guest container when that state is shutdown or reboot.
 *  Exit test for all guest container when system state is shutdown.
 *
 * @param [in]	cs		Pointer to containers_t
 * @return int
 * @retval  0 Success to change next state.
 * @retval -1 Got undefined state.
 */
int container_exec_internal_event(containers_t *cs)
{
	int num = 0;
	int ret = -1;
	int64_t timeout = 0;
	container_config_t *cc = NULL;

	num = cs->num_of_container;
	timeout = get_current_time_ms();

	if (cs->sys_state == CM_SYSTEM_STATE_RUN) {
		// internal event for run state

		// Check need to auto relaunch
		for(int i=0;i < num;i++) {
			cc = cs->containers[i];
			if (cc->runtime_stat.status == CONTAINER_DEAD) {
				// Dead state -> relaunch
				ret = container_restart(cc);
				if (ret == 0) {
					#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
					fprintf(stderr,"[CM CRITICAL ERROR] container %s relaunched.\n", cc->name);
					#endif

					ret = container_monitor_addguest(cs, cc);
					if (ret < 0) {
						// Can run guest without monitor, critical log only.
						#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
						fprintf(stderr,"[CM CRITICAL ERROR] Fail container_monitoring to %s ret = %d\n", cc->name, ret);
						#endif
					}

					// re-assign dynamic device
					// dynamic device update - if these return error, recover to update timing
					(void) container_all_dynamic_device_update_notification(cs);
				}
			} else if (cc->runtime_stat.status == CONTAINER_NOT_STARTED) {
				// checking to enabled container guest in own role
				container_config_t *active_cc = NULL;
				char *role = cc->role;

				// Find own role
				ret = container_get_active_guest_by_role(cs, role, &active_cc);
				if (ret == 0 && cc != active_cc) {
					// When cc != active_cc, change active guest cc to active_cc and disable cc.
					cc->runtime_stat.status = CONTAINER_DISABLE;
					(void) container_cleanup(cc);

					// Enable active_cc
					active_cc->runtime_stat.status = CONTAINER_NOT_STARTED;
					ret = container_start(active_cc);
					if (ret == 0) {
						#ifdef _PRINTF_DEBUG_
						fprintf(stdout,"container_start: Container guest %s was launched. role = %s\n", active_cc->name, role);
						#endif

						ret = container_monitor_addguest(cs, active_cc);
						if (ret < 0) {
							// Can run guest with out monitor, critical log only.
							#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
							fprintf(stderr,"[CM CRITICAL ERROR] container_start: container_monitor_addguest ret = %d\n", ret);
							#endif
						}
						// re-assign dynamic device
						// dynamic device update - if these return error, recover to update timing
						(void) container_all_dynamic_device_update_notification(cs);
					}
				}
			}
		}

		// Check to all container shutdown timeout.
		for(int i=0;i < num;i++) {
			cc = cs->containers[i];

			if (cc->runtime_stat.status == CONTAINER_SHUTDOWN || cc->runtime_stat.status == CONTAINER_REBOOT) {
				if (cc->runtime_stat.timeout < timeout) {
					// force kill after timeout
					(void) lxcutil_container_forcekill(cc);
					(void) container_terminate(cc);
					if (cc->runtime_stat.status == CONTAINER_REBOOT) {
						cc->runtime_stat.status = CONTAINER_DEAD; // Guest status change to dead. (For rebooting)
					} else {
						cc->runtime_stat.status = CONTAINER_NOT_STARTED; // Guest status change to not started. (For switching or stop)
					}
					#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
					fprintf(stderr,"[CM CRITICAL ERROR] container %s was shutdown/reboot timeout, fourcekill.\n", cc->name);
					#endif
				}
			}
		}

	} else if (cs->sys_state == CM_SYSTEM_STATE_SHUTDOWN) {
		// internal event for shutdown state
		int exit_count = 0;

		// Check to all container was exited.
		for(int i=0;i < num;i++) {
			cc = cs->containers[i];
			if (cc->runtime_stat.status == CONTAINER_EXIT || cc->runtime_stat.status == CONTAINER_DISABLE) {
				exit_count++;
			}
		}

		if (exit_count == num) {
			// All guest exited
			(void) container_mngsm_exit(cs);
			goto out;
		}

		// Check to all container shutdown timeout.
		for(int i=0;i < num;i++) {
			cc = cs->containers[i];
			if (cc->runtime_stat.status == CONTAINER_SHUTDOWN || cc->runtime_stat.status == CONTAINER_REBOOT) {
				if (cc->runtime_stat.timeout < timeout) {
					// force kill after timeout
					(void) lxcutil_container_forcekill(cc);
					(void) container_terminate(cc);
					cc->runtime_stat.status = CONTAINER_EXIT; // guest is force dead
					#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
					fprintf(stderr,"[CM CRITICAL ERROR] container %s was shutdown timeout at sys shutdown, fourcekill.\n", cc->name);
					#endif
				}
			}
		}
	} else {
		// undefined state
		return -1;
	}

out:
	return 0;
}
/**
 * Container launch common part of start and restart.
 *
 * @param [in]	cc	Pointer to container_config_t.
 * @return int
 * @retval  0 Success.
 * @retval -1 Create instance fail.
 * @retval -2 Container start fail.
 */
int container_restart(container_config_t *cc)
{
	int ret = -1;
	bool bret = false;

	// create container instance
	ret = lxcutil_create_instance(cc);
	if (ret < 0) {
		cc->runtime_stat.status = CONTAINER_DEAD;

		#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
		fprintf(stderr,"[CM CRITICAL ERROR] container_restart: lxcutil_create_instance ret = %d\n", ret);
		#endif
		return -1;
	}

	// Start container
	bret = cc->runtime_stat.lxc->start(cc->runtime_stat.lxc, 0, NULL);
	if (bret == false) {
		(void) lxcutil_release_instance(cc);
		cc->runtime_stat.status = CONTAINER_DEAD;

		// In case of relaunch, 'start' is fail while executing cleanup method by lxc-monitor.
		//Shall not out error message in this point.
		return -2;
	}

	cc->runtime_stat.status = CONTAINER_STARTED;

	return 0;
}
/**
 * Container start up
 *
 * @param [in]	cc	Pointer to container_config_t.
 * @return int
 * @retval  1 Container is disabled.
 * @retval  0 Success.
 * @retval -1 Critical error.
 * @retval -2 Target container is disable.
 */
int container_start(container_config_t *cc)
{
	int ret = -1;

	if (cc->runtime_stat.status == CONTAINER_DISABLE) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "container %s is disable launch\n", cc->name);
		#endif
		return -2;
	}

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr, "container_start %s\n", cc->name);
	#endif

	// run preprocess
	ret = container_start_preprocess_base(&cc->baseconfig);
	if (ret < 0) {
		#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
		fprintf(stderr,"[CM CRITICAL ERROR] container_start: container_start_preprocess_base ret = %d\n", ret);
		#endif
		return -1;
	}

	// Start container
	ret = container_restart(cc);
	if (ret < 0) {
		cc->runtime_stat.status = CONTAINER_DEAD;

		if (ret == -2) {
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			fprintf(stderr,"[CM CRITICAL ERROR] container_start: lxc-start fail %s\n", cc->name);
			#endif
		}
		return -1;
	}

	cc->runtime_stat.status = CONTAINER_STARTED;

	return 0;
}
/**
 * Get active guest container in selected role.
 *
 * @param [in]	cs			Pointer to containers_t.
 * @param [in]	role		role name.
 * @param [out]	active_cc	Double pointer to container_config_t. That is set pointer to active container config (container_config_t) in selected role.
 * @return int
 * @retval  0 Success.
 * @retval -1 No active guest.
 */
static int container_get_active_guest_by_role(containers_t *cs, char *role, container_config_t **active_cc)
{
	container_manager_role_config_t *cmrc = NULL;

	dl_list_for_each(cmrc, &cs->cmcfg->role_list, container_manager_role_config_t, list) {
		if (cmrc->name != NULL) {
			container_manager_role_elem_t *pelem = NULL;

			if (strcmp(cmrc->name, role) != 0)
				continue;

			pelem = dl_list_first(&cmrc->container_list, container_manager_role_elem_t, list) ;
			if (pelem != NULL) {
				if (pelem->cc != NULL) {
					(*active_cc) = pelem->cc;
					return 0;
				}
			}

			return -1;
		}
	}

	return -1;
}
/**
 * Container start up by role.
 *
 * @param [in]	cs		Pointer to containers_t.
 * @param [in]	role	role name.
 * @return int
 * @retval  0 Success to start gest.
 * @retval -1 Critical error.
 * @retval -2 No active guest.
 */
int container_start_by_role(containers_t *cs, char *role)
{
	container_config_t *cc = NULL;
	int ret = -1;
	int result = -2;

	ret = container_get_active_guest_by_role(cs, role, &cc);
	if (ret == 0) {
		// Got active guest
		cc->runtime_stat.status = CONTAINER_NOT_STARTED;

		ret = container_start(cc);
		if (ret == 0) {
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"container_start: Container guest %s was launched. role = %s\n", cc->name, role);
			#endif

			ret = container_monitor_addguest(cs, cc);
			if (ret < 0) {
				// Can run guest with out monitor, critical log only.
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				fprintf(stderr,"[CM CRITICAL ERROR] container_start: container_monitor_addguest ret = %d\n", ret);
				#endif
			}
			result = 0;
		} else {
			result = -1;
		}
	} else {
		result = -2;
	}

	return result;
}
/**
 * All device update notification for all container
 * For force device assignment to new container guest.
 *
 * @param [in]	cs	Preconstructed containers_t.
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
int container_all_dynamic_device_update_notification(containers_t *cs)
{
	int ret = 1;
	container_control_interface_t *cci = NULL;

	ret = container_mngsm_interface_get(&cci, cs);
	if (ret < 0) {
		// May not get this error
		return -1;
	}

	// Async dynamic device update - if these return error, recover to update timing
	(void) cci->netif_updated(cci);

	return 0;
}
/**
 * This function use in case of container terminated such as dead and exit.
 * This function remove per container runtime resource and remove assigned dynamic devices from container_config_t.
 *
 * @param [in]	cc	Pointer to container_config_t.
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.(Reserve)
 */
int container_terminate(container_config_t *cc)
{
	(void) lxcutil_release_instance(cc);
	(void) container_netif_remove_element(cc);

	return 0;
}
/**
 * This function is preprocess for container manager exit and post process for runtime shutdown.
 * This function exec to filesystem unmount and same function of container_terminate.
 *
 * @param [in]	cc	Pointer to container_config_t.
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.(Reserve)
 */
int container_cleanup(container_config_t *cc)
{
	(void) container_terminate(cc);
	(void) container_cleanup_preprocess_base(&cc->baseconfig);

	return 0;
}
/**
 * Disk mount procedure for failover.
 * This function is sub function for container_start_preprocess_base.
 *
 * @param [in]	devs	Array of disk block device. A and B.
 * @param [in]	path	Mount path.
 * @param [in]	fstype	Name of file system. When fstype == NULL, file system is auto.
 * @param [in]	mntflag	Mount flag.
 * @return int
 * @retval  1 Success - secondary.
 * @retval  0 Success - primary.
 * @retval -1 mount error.
 * @retval -2 Syscall error.
 * @retval -3 Arg. error.
 */
static int container_start_mountdisk_failover(char **devs, const char *path, const char *fstype, unsigned long mntflag)
{
	int ret = -1;
	int mntdisk = -1;
	const char * dev = NULL;

	for (int i=0; i < 2; i++) {
		dev = devs[i];

		ret = mount(dev, path, fstype, mntflag, NULL);
		if (ret < 0) {
			if (errno == EBUSY) {
				// already mounted
				#ifdef _PRINTF_DEBUG_
				fprintf(stderr,"container_start_preprocess_base: %s is already mounted.\n", path);
				#endif
				ret = umount2(path, MNT_DETACH);
				if (ret < 0) {
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"container_start_preprocess_base: %s unmount fail.\n", path);
					#endif
					continue;
				}

				ret = mount(dev, path, fstype, mntflag, NULL);
				if (ret < 0) {
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"container_start_preprocess_base: %s re-mount fail.\n", path);
					#endif
					continue;
				}
				mntdisk = i;
				break;
			} else {
				//error - try to mount secondary disk
				;
			}
		} else {
			// success to mount
			mntdisk = i;
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"container_start_mountdisk_failover: mounted %s to %s.\n", dev, path);
			#endif
			break;
		}
	}

	return mntdisk;
}
/**
 * Disk mount procedure for a/b.
 * This function is sub function for container_start_preprocess_base.
 *
 * @param [in]	devs	Array of disk block device. A and B.
 * @param [in]	path	Mount path.
 * @param [in]	fstype	Name of file system. When fstype == NULL, file system is auto.
 * @param [in]	mntflag	Mount flag.
 * @param [in]	side	Mount side a(=0) or b(=1).
 * @return int
 * @retval  0 Success.
 * @retval -1 mount error.
 * @retval -2 Syscall error.
 * @retval -3 Arg. error.
 */
static int container_start_mountdisk_ab(char **devs, const char *path, const char *fstype, unsigned long mntflag, int side)
{
	int ret = 1;
	const char * dev = NULL;

	if (side < 0 || side > 2)
		return -3;

	dev = devs[side];

	ret = mount(dev, path, fstype, mntflag, NULL);
	if (ret < 0) {
		if (errno == EBUSY) {
			// already mounted
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"container_start_mountdisk_ab: %s is already mounted.\n", path);
			#endif
			ret = umount2(path, MNT_DETACH);
			if (ret < 0) {
				#ifdef _PRINTF_DEBUG_
				fprintf(stderr,"container_start_mountdisk_ab: %s unmount fail.\n", path);
				#endif
				return -1;
			}

			ret = mount(dev, path, fstype, mntflag, NULL);
			if (ret < 0) {
				#ifdef _PRINTF_DEBUG_
				fprintf(stderr,"container_start_mountdisk_ab: %s re-mount fail.\n", path);
				#endif
				return -1;
			}
		} else {
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"container_start_mountdisk_ab: %s mount fail to %s (%d).\n", dev, path, errno);
			#endif
			return -1;
		}
	}

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr,"container_start_mountdisk_ab(%d): %s mount to %s (%s)\n", side, dev, path, fstype);
	#endif

	return 0;
}


/**
 * Preprocess for container start.
 * This function exec mount operation a part of base config operation.
 *
 * @param [in]	bc	Pointer to container_baseconfig_t.
 * @return int
 * @retval  0 Success.
 * @retval -1 mount error.
 * @retval -2 Syscall error.
 */
static int container_start_preprocess_base(container_baseconfig_t *bc)
{
	int ret = 1;
	unsigned long mntflag = 0;

	// mount rootfs
	if (bc->rootfs.mode == DISKMOUNT_TYPE_RW) {
		mntflag = MS_DIRSYNC | MS_NOATIME | MS_NODEV | MS_SYNCHRONOUS;
	} else {
		mntflag = MS_NOATIME | MS_RDONLY;
	}

	ret = container_start_mountdisk_ab(bc->rootfs.blockdev, bc->rootfs.path
										, bc->rootfs.filesystem, mntflag, bc->abboot);
	if ( ret < 0) {
		// root fs mount is mandatory.
		#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
		fprintf(stderr,"[CM CRITICAL ERROR] container_start_preprocess_base: mandatory disk %s could not mount\n", bc->rootfs.blockdev[bc->abboot]);
		#endif
		return -1;
	}

	// mount extradisk - optional
	if (!dl_list_empty(&bc->extradisk_list)) {
		container_baseconfig_extradisk_t *exdisk = NULL;

		dl_list_for_each(exdisk, &bc->extradisk_list, container_baseconfig_extradisk_t, list) {
			if (exdisk->mode == DISKMOUNT_TYPE_RW) {
				mntflag = MS_DIRSYNC | MS_NOATIME | MS_NODEV | MS_NOEXEC | MS_SYNCHRONOUS;
			} else {
				mntflag = MS_NOATIME | MS_RDONLY;
			}

			if (exdisk->redundancy == DISKREDUNDANCY_TYPE_AB)
			{
				ret = container_start_mountdisk_ab(exdisk->blockdev, exdisk->from, exdisk->filesystem, mntflag, bc->abboot);
				if (ret < 0) {
					// AB disk mount is mandatory function.
					#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
					fprintf(stderr,"[CM CRITICAL ERROR] container_start_preprocess_base: mandatory disk %s could not mount\n", exdisk->blockdev[bc->abboot]);
					#endif
					return -1;
				}
			} else {
				ret = container_start_mountdisk_failover(exdisk->blockdev, exdisk->from, exdisk->filesystem, mntflag);
				if (ret < 0) {
					// Failover disk mount is optional function.
					#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
					fprintf(stderr,"[CM ERROR] container_start_preprocess_base: failover disk %s could not mount\n", exdisk->blockdev[0]);
					#endif
					continue;
				}
			}

		}
	}

	return 0;
}
/**
 * Cleanup for container start base preprocess.
 * This function exec unmount operation a part of base config cleanup operation.
 *
 * @param [in]	bc	Pointer to container_baseconfig_t.
 * @return int
 * @retval  0 Success.
 * @retval -1 unmount error.
 * @retval -2 Syscall error.
 */
static int container_cleanup_preprocess_base(container_baseconfig_t *bc)
{
	int ret = -1;

	// unmount extradisk
	if (!dl_list_empty(&bc->extradisk_list)) {
		container_baseconfig_extradisk_t *exdisk = NULL;

		dl_list_for_each(exdisk, &bc->extradisk_list, container_baseconfig_extradisk_t, list) {

			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"container_cleanup_preprocess_base: unmount to %s.\n", exdisk->from);
			#endif
			ret = umount(exdisk->from);
			if (ret < 0) {
				if (errno == EBUSY) {
					(void) umount2(exdisk->from, MNT_DETACH);
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"container_cleanup_preprocess_base: lazy unmount to %s.\n", exdisk->from);
					#endif
				}
			}
		}
	}

	// unmount rootfs
	#ifdef _PRINTF_DEBUG_
	fprintf(stderr,"container_cleanup_preprocess_base: unmount to rootfs %s.\n", bc->rootfs.path);
	#endif
	ret = umount(bc->rootfs.path);
	if (ret < 0) {
		if (errno == EBUSY) {
			(void) umount2(bc->rootfs.path, MNT_DETACH);
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"container_cleanup_preprocess_base: lazy unmount to rootfs %s.\n", bc->rootfs.path);
			#endif
		}
	}

	return 0;
}