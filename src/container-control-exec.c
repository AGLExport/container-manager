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
#include "container-workqueue.h"
#include "device-control.h"

static int container_start_preprocess_base(container_baseconfig_t *bc);
static int container_start_preprocess_base_recovery(container_config_t *cc);
static int container_cleanup_preprocess_base(container_baseconfig_t *bc, int64_t timeout);
static int container_get_active_guest_by_role(containers_t *cs, char *role, container_config_t **active_cc);
static int container_timeout_set(container_config_t *cc);
static int container_setup_delayed_operation(container_config_t *cc);
static int container_do_delayed_operation(container_config_t *cc);
static int container_cleanup_delayed_operation(container_config_t *cc);

/**
 * @def	g_reduced_critical_error_mount
 * @brief	Error log output rate. The type of mount retry error should be reduced using this parameter.
 */
static const int g_reduced_critical_error_mount = 100;
/**
 * @def	g_reduced_critical_error_launch
 * @brief	Error log output rate. The type of launch retry error should be reduced using this parameter.
 */
static const int g_reduced_critical_error_launch = 100;

/**
 * The function for timeout calculate and set.
 *
 * @param [in]	cc	Pointer to container_config_t.
 * @return int
 * @retval  0 Success to operation.
 * @retval -1 Critical error.(Reserve)
 */
static int container_timeout_set(container_config_t *cc)
{
	int64_t timeout = 0;

	// Set timeout
	timeout = get_current_time_ms();
	if (timeout < 0) {
		timeout = 0;
	}
	timeout = timeout + cc->baseconfig.lifecycle.timeout;
	cc->runtime_stat.timeout = timeout;

	return 0;
}

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

		if (nii->ifindex <= 0) {
			continue;
		}

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
					(void) fprintf(stdout, "[fail] network if update add %s to %s\n", cdne->ifname, cc->name);
					#endif
				} else {
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout, "network if update add %s to %s\n", cdne->ifname, cc->name);
					#endif
					// nop
					;
				}
			} else if (cdne->ifindex == nii->ifindex) {
				// existing netif
				cdne->is_available = 1;
			} else {
				// nop
				;
			}
		}
	}

	// disable removed netif
	dl_list_for_each(cdne, &cdn->dynamic_netiflist, container_dynamic_netif_elem_t, list) {
		if (cdne->is_available == 0 && cdne->ifindex != 0) {
			cdne->ifindex = 0;

			// Don't need memory free.
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "network if update removed %s from %s\n", cdne->ifname, cc->name);
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
		#ifdef _PRINTF_DEBUG_
		if (cdne->ifindex != 0) {
			(void) fprintf(stdout, "network if update removed %s from %s\n", cdne->ifname, cc->name);
		}
		#endif

		cdne->ifindex = 0;
		// Don't need memory free.
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
		if (ret < 0) {
			goto err_ret;
		}
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
int container_exited(containers_t *cs, const container_mngsm_guest_exit_data_t *data)
{
	int num = 0, container_num = 0;
	int result = 0;
	container_config_t *cc = NULL;

	num = cs->num_of_container;
	container_num = data->container_number;
	if ((container_num < 0) || (num <= container_num)) {
		return -1;
	}

	cc = cs->containers[container_num];

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout,"container_exited : %s\n", cc->name);
	#endif

	if (cs->sys_state  == CM_SYSTEM_STATE_RUN) {
		if (cc->runtime_stat.status == CONTAINER_NOT_STARTED) {
			// not running, not need shutdown
			;
		} else if (cc->runtime_stat.status == CONTAINER_STARTED) {
			// now running, guest was dead
			cc->runtime_stat.status = CONTAINER_DEAD;

			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			(void) fprintf(stderr,"[CM CRITICAL INFO] container %s was dead.\n", cc->name);
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
		} else if (cc->runtime_stat.status == CONTAINER_RUN_WORKER) {
			// not running, not need shutdown. same as CONTAINER_NOT_STARTED.
			;
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
		} else if (cc->runtime_stat.status == CONTAINER_RUN_WORKER) {
			// May not get this state, change to exit state. (fail safe)
			cc->runtime_stat.status = CONTAINER_EXIT;
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
	(void) fprintf(stdout,"container_request_shutdown to %s (%d)\n", cc->name, sys_state);
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
				(void) container_terminate(cc);
				cc->runtime_stat.status = CONTAINER_NOT_STARTED; // guest is force dead
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				(void) fprintf(stderr,"[CM CRITICAL ERROR] At container_request_shutdown fourcekill to %s.\n", cc->name);
				#endif
			} else {
				(void) container_timeout_set(cc);
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
		} else if (cc->runtime_stat.status == CONTAINER_RUN_WORKER) {
			// Now working container worker, already shutdown, not need new action
			;
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
				(void) container_terminate(cc);
				cc->runtime_stat.status = CONTAINER_EXIT; // guest is force exit
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				(void) fprintf(stderr,"[CM CRITICAL ERROR] At container_request_shutdown fourcekill to %s.\n", cc->name);
				#endif
			} else {
				(void) container_timeout_set(cc);
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
		} else if (cc->runtime_stat.status == CONTAINER_RUN_WORKER) {
			// Now working container worker, need to cancel worker.
			(void) container_workqueue_cancel(&cc->workqueue);
			(void) container_timeout_set(cc);

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
	(void) fprintf(stdout,"container_request_reboot to %s (%d)\n", cc->name, sys_state);
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
				(void) container_terminate(cc);
				cc->runtime_stat.status = CONTAINER_DEAD; // guest is force dead
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				(void) fprintf(stderr,"[CM CRITICAL ERROR] At container_request_reboot fourcekill to %s.\n", cc->name);
				#endif
			} else {
				//Requested, wait to exit.
				(void) container_timeout_set(cc);

				cc->runtime_stat.status = CONTAINER_REBOOT;
			}
		} else if (cc->runtime_stat.status == CONTAINER_REBOOT) {
			// When reboot state, state not need to change.
			;
		} else if (cc->runtime_stat.status == CONTAINER_SHUTDOWN) {
			// Already requested shutdown, not need new action.
			;
		} else if (cc->runtime_stat.status == CONTAINER_DEAD) {
			// Already dead container, shall be re-launch - no change state.
			;
		} else if (cc->runtime_stat.status == CONTAINER_EXIT) {
			// undefined state
			result = -1;
		} else if (cc->runtime_stat.status == CONTAINER_RUN_WORKER) {
			// Now working container worker, shall not change new state.
			;
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
				(void) container_terminate(cc);
				cc->runtime_stat.status = CONTAINER_EXIT; // guest is force exit
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				(void) fprintf(stderr,"[CM CRITICAL ERROR] At container_request_shutdown fourcekill to %s.\n", cc->name);
				#endif
			} else {
				// Set timeout
				(void) container_timeout_set(cc);

				//Requested, wait to exit.
				cc->runtime_stat.status = CONTAINER_SHUTDOWN;
			}
		} else if (cc->runtime_stat.status == CONTAINER_REBOOT) {
			// Already requested reboot, not need new action.
			;
		} else if (cc->runtime_stat.status == CONTAINER_SHUTDOWN) {
			// Already requested shutdown, not need new action.
			;
		} else if (cc->runtime_stat.status == CONTAINER_DEAD) {
			// Already dead container, it's already exit.
			cc->runtime_stat.status = CONTAINER_EXIT;
		} else if (cc->runtime_stat.status == CONTAINER_EXIT) {
			// Already exit, not need new action
			;
		} else if (cc->runtime_stat.status == CONTAINER_RUN_WORKER) {
			// Now working container worker, need to cancel worker.
			(void) container_workqueue_cancel(&cc->workqueue);
			(void) container_timeout_set(cc);

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

	if (fail_count > 0) {
		return -1;
	}

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
				ret = container_start(cc);
				if (ret == 0) {
					#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
					(void) fprintf(stderr,"[CM CRITICAL INFO] container %s relaunched.\n", cc->name);
					#endif

					ret = container_monitor_addguest(cs, cc);
					if (ret < 0) {
						// Can run guest without monitor, critical log only.
						#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
						(void) fprintf(stderr,"[CM CRITICAL ERROR] Fail container_monitoring to %s ret = %d\n", cc->name, ret);
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
				if (ret == 0) {
					if (cc != active_cc) {
						// When cc != active_cc, change active guest cc to active_cc and disable cc.
						cc->runtime_stat.status = CONTAINER_DISABLE;
						(void) container_cleanup(cc, 0);

						// Enable active_cc
						active_cc->runtime_stat.status = CONTAINER_NOT_STARTED;
						ret = container_start(active_cc);
						if (ret == 0) {
							#ifdef _PRINTF_DEBUG_
							(void) fprintf(stdout,"container_start: Container guest %s was launched. role = %s\n", active_cc->name, role);
							#endif

							ret = container_monitor_addguest(cs, active_cc);
							if (ret < 0) {
								// Can run guest with out monitor, critical log only.
								#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
								(void) fprintf(stderr,"[CM CRITICAL ERROR] Fail container_monitoring to %s ret = %d\n", active_cc->name, ret);
								#endif
							}
							// re-assign dynamic device
							// dynamic device update - if these return error, recover to update timing
							(void) container_all_dynamic_device_update_notification(cs);
						}
					} else {
						// When cc == active_cc and per container workqueue is active, exec per container workqueue operation.
						int status = -1;
						(void) container_cleanup(cc, 200);

						status = container_workqueue_get_status(&cc->workqueue);
						if (status == CONTAINER_WORKER_SCHEDULED) {
							// A workqueue is scheduled, run that.
							ret = container_workqueue_run(&cc->workqueue);
							if (ret < 0) {
								if ((ret == -2) || (ret == -3)) {
									// Remove work queue
									#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
									(void) fprintf(stderr,"[CM CRITICAL ERROR] Fail to container workqueue run ret = %d at %s\n", ret, cc->name);
									#endif

									ret = container_workqueue_cancel(&cc->workqueue);
									if (ret <= 0) {
										int after_execute = 0;

										(void) container_workqueue_remove(&cc->workqueue, &after_execute);
										if (after_execute == 1) {
											// When CONTAINER_DEAD set to runtime_stat.status, this container will restart in next event execution cycle.
											cc->runtime_stat.status = CONTAINER_DEAD;
										} else {
											cc->runtime_stat.status = CONTAINER_NOT_STARTED;
										}
										#ifdef _PRINTF_DEBUG_
										(void) fprintf(stdout, "container_workqueue cancel and remove at %s\n", cc->name);
										#endif
									} else {
										;	//nop
									}
								}
							} else {
								// Change state to CONTAINER_RUN_WORKER
								cc->runtime_stat.status = CONTAINER_RUN_WORKER;
							}

						} else {
							;	// nop
						}
					}
				}
			} else if (cc->runtime_stat.status == CONTAINER_RUN_WORKER) {
				// Now run worker
				int status = -1;

				status = container_workqueue_get_status(&cc->workqueue);
				if (status == CONTAINER_WORKER_COMPLETED) {
					// A workqueue is completed, cleanup and set next state.
					int after_execute = 0;

					ret = container_workqueue_cleanup(&cc->workqueue, &after_execute);
					if (ret == 0) {
						if (after_execute == 1) {
							// When CONTAINER_DEAD set to runtime_stat.status, this container will restart in next event execution cycle.
							cc->runtime_stat.status = CONTAINER_DEAD;
						} else {
							cc->runtime_stat.status = CONTAINER_NOT_STARTED;
						}
						#ifdef _PRINTF_DEBUG_
						(void) fprintf(stdout, "container_workqueue end of worker exec(%d) at %s\n", after_execute, cc->name);
						#endif
					} else {
						;	// nop
					}
				} else {
					;	// nop
				}
			} else {
				// nop
				;
			}
		}

		// Check to all container shutdown timeout.
		for(int i=0;i < num;i++) {
			cc = cs->containers[i];

			if ((cc->runtime_stat.status == CONTAINER_SHUTDOWN) || (cc->runtime_stat.status == CONTAINER_REBOOT)) {
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
					(void) fprintf(stderr,"[CM CRITICAL INFO] container %s was shutdown/reboot timeout, fourcekill.\n", cc->name);
					#endif
				}
			}
		}

		// Do delayed operation to all container. Only to exec CM_SYSTEM_STATE_RUN.
		for(int i=0;i < num;i++) {
			cc = cs->containers[i];
			(void) container_do_delayed_operation(cc);
		}

		// Do cyclic operation for manager.
		(void) container_mngsm_do_cyclic_operation(cs);

	} else if (cs->sys_state == CM_SYSTEM_STATE_SHUTDOWN) {
		// internal event for shutdown state
		int exit_count = 0;

		// Check to all container was exited.
		for(int i=0;i < num;i++) {
			cc = cs->containers[i];
			if ((cc->runtime_stat.status == CONTAINER_EXIT) || (cc->runtime_stat.status == CONTAINER_DISABLE)) {
				exit_count++;
			} else if (cc->runtime_stat.status == CONTAINER_RUN_WORKER) {
				// Now run worker
				int after_execute = 0;	//dummy

				ret = container_workqueue_cleanup(&cc->workqueue, &after_execute);
				if (ret == 0) {
					// A workqueue is completed and cleanup success.
					cc->runtime_stat.status = CONTAINER_EXIT;

					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout, "container_workqueue end of worker exec(%d) at %s\n", after_execute, cc->name);
					#endif
				} else {
					// A workqueue is not completed. Fail safe : send cancel request.
					(void) container_workqueue_cancel(&cc->workqueue);
				}
			} else {
				;	//nop
			}
		}

		if (exit_count == num) {
			// All guest exited
			// Do manager terminate operation.
			ret = container_mngsm_exec_delayed_operation(cs, 1);
			if (ret == -1) {
				// Now run. wait worker complete.
				ret = container_mngsm_do_cyclic_operation(cs);
				if (ret == 1) {
					//Complete worker.
					(void) container_mngsm_exit(cs);
				}
			} else if (ret == 1) {
				// Operation is not required.
				(void) container_mngsm_exit(cs);
			} else if (ret == 0) {
				;	//nop, now started
			} else {
				(void) container_mngsm_exit(cs);
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				(void) fprintf(stderr,"[CM CRITICAL ERROR] Fail to container mngsm worker execution.\n");
				#endif
			}
			goto out;
		}

		// Check to all container shutdown timeout.
		for(int i=0;i < num;i++) {
			cc = cs->containers[i];
			if ((cc->runtime_stat.status == CONTAINER_SHUTDOWN) || (cc->runtime_stat.status == CONTAINER_REBOOT)) {
				if (cc->runtime_stat.timeout < timeout) {
					// force kill after timeout
					(void) lxcutil_container_forcekill(cc);
					(void) container_terminate(cc);
					cc->runtime_stat.status = CONTAINER_EXIT; // guest is force dead
					#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
					(void) fprintf(stderr,"[CM CRITICAL INFO] container %s was shutdown timeout at sys shutdown, fourcekill.\n", cc->name);
					#endif
				}
			} else if (cc->runtime_stat.status == CONTAINER_RUN_WORKER) {
				if (cc->runtime_stat.timeout < timeout) {
					// force state change
					cc->runtime_stat.status = CONTAINER_EXIT;
				}
			} else {
				;	//nop
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
 * Container launch operation for container start.
 *
 * @param [in]	cc	Pointer to container_config_t.
 * @return int
 * @retval  0 Success.
 * @retval -1 Create instance fail.
 * @retval -2 Container start fail.
 */
static int container_launch(container_config_t *cc)
{
	int ret = -1;
	bool bret = false;

	// create container instance
	ret = lxcutil_create_instance(cc);
	if (ret < 0) {
		cc->runtime_stat.status = CONTAINER_DEAD;

		#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
		(void) fprintf(stderr,"[CM CRITICAL ERROR] lxcutil_create_instance ret = %d\n", ret);
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
		(void) fprintf(stdout, "container %s is disable launch\n", cc->name);
		#endif
		return -2;
	}

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout, "container_start %s\n", cc->name);
	#endif

	// run preprocess
	ret = container_start_preprocess_base(&cc->baseconfig);
	if (ret < 0) {
		// When got error from container_start_preprocess_base, try to evaluate recovery.

		(void) container_start_preprocess_base_recovery(cc);
		// Don't care for result. Need to retry container start.

		return -1;
	}

	ret = container_setup_delayed_operation(cc);
	if (ret < 0) {
		// May not get this error.
		#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
		(void) fprintf(stderr,"[CM CRITICAL ERROR] Delayed operation setup fail in %s.\n", cc->name);
		#endif

		return -1;
	}

	// Launch container
	ret = container_launch(cc);
	if (ret < 0) {
		cc->runtime_stat.status = CONTAINER_DEAD;

		if (ret == -2) {
			cc->runtime_stat.launch_error_count = cc->runtime_stat.launch_error_count + 1;
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			if ((cc->runtime_stat.launch_error_count %g_reduced_critical_error_launch) == 1) {
				(void) fprintf(stderr,"[CM CRITICAL ERROR] container %s start fail.\n", cc->name);
			}
			#endif
		}
		return -1;
	}

	cc->runtime_stat.status = CONTAINER_STARTED;
	#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
	if (cc->runtime_stat.launch_error_count > 0) {
		// When success to launch after error, out extra log.
		(void) fprintf(stderr,"[CM CRITICAL INFO] Revival container launch after %d errs.\n", cc->runtime_stat.launch_error_count);
	}
	#endif
	cc->runtime_stat.launch_error_count = 0;

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

			if (strcmp(cmrc->name, role) != 0) {
				continue;
			}

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
			(void) fprintf(stdout,"container_start: Container guest %s was launched. role = %s\n", cc->name, role);
			#endif

			ret = container_monitor_addguest(cs, cc);
			if (ret < 0) {
				// Can run guest with out monitor, critical log only.
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				(void) fprintf(stderr,"[CM CRITICAL ERROR] Fail container_monitoring to %s ret = %d\n", cc->name, ret);
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
 * @param [in]	cc		Pointer to container_config_t.
 * @param [in]	timeout	The timeout (ms) -  relative.  When timeout is less than 1, it will not do internal retry.
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.(Reserve)
 */
int container_cleanup(container_config_t *cc, int64_t timeout)
{
	(void) container_terminate(cc);
	(void) container_cleanup_delayed_operation(cc);
	(void) container_cleanup_preprocess_base(&cc->baseconfig, timeout);

	return 0;
}
/**
 * Rootfs mount procedure.
 * This function do mount operation for guest rootfilesystem.
 *
 * @param [in]	bc	Pointer to container_baseconfig_rootfs_t.
* @param [in]	side	Mount side a(=0) or b(=1).
 * @return int
 * @retval  0 Success.
 * @retval -1 mount error.
 * @retval -2 Syscall error.
 * @retval -3 Device type error.
 */
static int container_start_preprocess_base_do_mount_rootfs(container_baseconfig_rootfs_t *cbr, int side)
{
	int ret = -1, result = 0;

	// mount rootfs
	if (cbr->device_type == DEVICE_TYPE_BLOCK) {
		// Device is block device.
		unsigned long mntflag = 0;
		if (cbr->mode == DISKMOUNT_TYPE_RW) {
			mntflag = (MS_DIRSYNC | MS_NOATIME | MS_NODEV | MS_SYNCHRONOUS);
		} else {
			mntflag = (MS_NOATIME | MS_RDONLY);
		}

		ret = mount_disk_ab(cbr->rootfs_dev, cbr->path
							, cbr->filesystem, mntflag, cbr->option, side);
		if (ret < 0) {
			result = -1;
		}
	} else if (cbr->device_type == DEVICE_TYPE_HOST_ROOTFILESYSTEM) {
		// Bind mount.
		int is_read_only = 0;
		if (cbr->mode == DISKMOUNT_TYPE_RO) {
			is_read_only = 1;
		} else {
			is_read_only = 0;
		}

		ret = mount_disk_bind(cbr->rootfs_dev[0], cbr->path, is_read_only);
		if (ret < 0) {
			result = -1;
		}
	} else {
		result = -3;
	}

	return result;
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
	if (bc->rootfs.is_mounted == 0) {
		// When already mounted, bypass this mount operation.
		ret = container_start_preprocess_base_do_mount_rootfs(&bc->rootfs, bc->abboot);
		if (ret < 0) {
			// root fs mount is mandatory.
			bc->rootfs.error_count = bc->rootfs.error_count + 1;
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			if ((bc->rootfs.error_count % g_reduced_critical_error_mount) == 1) {
				// This log should be reduced to one output per 100 time (default) of error.
				(void) fprintf(stderr
								,"[CM CRITICAL ERROR] Mandatory disk %s could not mount. (count = %d)\n"
								, bc->rootfs.rootfs_dev[bc->abboot], bc->rootfs.error_count);
			}
			#endif
			return -1;
		} else {
			// root fs mount is succeed.
			bc->rootfs.is_mounted = 1;
			// Clear error count
			bc->rootfs.error_count = 0;
		}
	}

	// mount extradisk - optional
	if (!dl_list_empty(&bc->extradisk_list)) {
		container_baseconfig_extradisk_t *exdisk = NULL;

		dl_list_for_each(exdisk, &bc->extradisk_list, container_baseconfig_extradisk_t, list) {
			if (exdisk->is_mounted == 0) {
				// When already mounted, bypass this mount operation.
				if (exdisk->mode == DISKMOUNT_TYPE_RW) {
					mntflag = MS_DIRSYNC | MS_NOATIME | MS_NODEV | MS_NOEXEC | MS_SYNCHRONOUS;
				} else {
					mntflag = MS_NOATIME | MS_RDONLY;
				}

				if (exdisk->redundancy == DISKREDUNDANCY_TYPE_AB)
				{
					ret = mount_disk_ab(exdisk->blockdev, exdisk->from, exdisk->filesystem, mntflag, exdisk->option, bc->abboot);
					if (ret < 0) {
						// AB disk mount is mandatory function.
						exdisk->error_count = exdisk->error_count + 1;
						#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
						if ((exdisk->error_count % g_reduced_critical_error_mount) == 1) {
							// This log should be reduced to one output per 100 time of error.
							(void) fprintf(stderr
											,"[CM CRITICAL ERROR] Extra ab mount disk %s could not mount. (count = %d)\n"
											, exdisk->blockdev[bc->abboot], exdisk->error_count);
						}
						#endif
						return -1;
					} else {
						// This extra disk mount is succeed.
						exdisk->is_mounted = 1;
						// Clear error count
						exdisk->error_count = 0;
					}
				} else if (exdisk->redundancy == DISKREDUNDANCY_TYPE_FAILOVER) {
					ret = mount_disk_failover(exdisk->blockdev, exdisk->from, exdisk->filesystem, mntflag, exdisk->option);
					if (ret < 0) {
						// Failover disk mount is optional function.
						exdisk->error_count = exdisk->error_count + 1;
						#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
						if ((exdisk->error_count % g_reduced_critical_error_mount) == 1) {
							// This log should be reduced to one output per 100 time of error.
							(void) fprintf(stderr
											,"[CM CRITICAL ERROR] Extra failover disk %s could not mount. (count = %d)\n"
											, exdisk->blockdev[0], exdisk->error_count);
						}
						#endif
						continue;
					} else {
						// This extra disk mount is succeed.
						exdisk->is_mounted = 1;
						// Clear error count
						exdisk->error_count = 0;
					}
				} else {
					// DISKREDUNDANCY_TYPE_FSCK or DISKREDUNDANCY_TYPE_MKFS
					ret = mount_disk_once(exdisk->blockdev, exdisk->from, exdisk->filesystem, mntflag, exdisk->option);
					if (ret < 0) {
						exdisk->error_count = exdisk->error_count + 1;
						// This point is critical error but not out critical error log. This log will out in recovery operation.
						#ifdef _PRINTF_DEBUG_
						if ((exdisk->error_count % g_reduced_critical_error_mount) == 1) {
							// This log should be reduced to one output per 100 time of error.
							(void) fprintf(stdout
											,"container_start_preprocess_base: disk %s could not mount. (count = %d)\n"
											, exdisk->blockdev[0], exdisk->error_count);
						}
						#endif
						return -1;
					} else {
						// This extra disk mount is succeed.
						exdisk->is_mounted = 1;
						// Clear error count
						exdisk->error_count = 0;
					}
				}
			}
		}
	}

	return 0;
}
/**
 * Preprocess for container start.
 * This function exec mount operation a part of base config operation.
 *
 * @param [in]	bc	Pointer to container_baseconfig_t.
 * @return int
 * @retval  1 Success (recovery queued).
 * @retval  0 Success (recovery not queued).
 * @retval -1 operation error.
 * @retval -2 Syscall error.
 */
static int container_start_preprocess_base_recovery(container_config_t *cc)
{
	int ret = 1;
	container_baseconfig_t *bc = NULL;

	bc = &cc->baseconfig;

	// rootfs does not support recovery now.

	// evaluate recovery fot extradisk
	if (!dl_list_empty(&bc->extradisk_list)) {
		container_baseconfig_extradisk_t *exdisk = NULL;

		dl_list_for_each(exdisk, &bc->extradisk_list, container_baseconfig_extradisk_t, list) {
			if (exdisk->is_mounted == 0) {
				// When already mounted, this disk is valid.
				if (exdisk->error_count > 0) {
					char option_str[1024];

					ret = snprintf(option_str, sizeof(option_str), "device=%s", exdisk->blockdev[0]);
					if (!((size_t)ret < sizeof(option_str)-1u)) {
						return -1;
					}

					if (exdisk->redundancy == DISKREDUNDANCY_TYPE_FSCK) {
						ret = container_workqueue_schedule(&cc->workqueue, "fsck", option_str, 1);
						if (ret == 0) {
							#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
							if ((exdisk->error_count % g_reduced_critical_error_mount) == 1) {
								// This log should be reduced to one output per 100 time (default) of error.
								(void) fprintf(stderr,"[CM CRITICAL ERROR] Queued fsck recovery to disk %s.\n", exdisk->blockdev[0]);
							}
							#endif
							return 1;
						}
					} else if (exdisk->redundancy == DISKREDUNDANCY_TYPE_MKFS) {
						ret = container_workqueue_schedule(&cc->workqueue, "mkfs", option_str, 1);
						if (ret == 0) {
							#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
							// This log is output every time. Because this operation is force recovery, may not fail cyclic.
							(void) fprintf(stderr,"[CM CRITICAL ERROR] Queued mkfs recovery to disk %s.\n", exdisk->blockdev[0]);
							#endif
							return 1;
						}
					} else {
						;	//no operation
					}
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
 * @param [in]	timeout	The timeout (ms) -  relative.  When timeout is less than 1, it will not do internal retry.
 * @return int
 * @retval  0 Success.
 * @retval -1 unmount error.
 * @retval -2 Syscall error.
 */
static int container_cleanup_preprocess_base(container_baseconfig_t *bc, int64_t timeout)
{
	int64_t timeout_time = 0, timeout_local = 0;
	int retry_max = 0;

	if (timeout < 0) {
		timeout_local = 0;
	} else {
		timeout_local = timeout;
	}

	timeout_time = get_current_time_ms() + timeout_local;
	retry_max = (timeout_local / 50) + 1;

	// unmount extradisk
	if (!dl_list_empty(&bc->extradisk_list)) {
		container_baseconfig_extradisk_t *exdisk = NULL;

		dl_list_for_each(exdisk, &bc->extradisk_list, container_baseconfig_extradisk_t, list) {

			if (exdisk->is_mounted != 0) {
				(void) unmount_disk(exdisk->from, timeout_time, retry_max);
				// Clear mount flag
				exdisk->is_mounted = 0;
				// Clear error count
				exdisk->error_count = 0;
			}
		}
	}

	// unmount rootfs
	if (bc->rootfs.is_mounted != 0) {
		(void) unmount_disk(bc->rootfs.path, timeout_time, retry_max);
		// Clear mount flag
		bc->rootfs.is_mounted = 0;
		// Clear error count
		bc->rootfs.error_count = 0;
	}

	return 0;
}

/**
 * Setup for the per container delayed operation.
 * This function setup for per container delayed operation such as delayed bind mount directory from host to guest.
 *
 * @param [in]	cc	Pointer to container_config_t.
 * @return int
 * @retval  0 Success.
 * @retval -1 Internal error.
 * @retval -2 Syscall error.
 */
static int container_setup_delayed_operation(container_config_t *cc)
{
	container_delayed_mount_elem_t *dmelem = NULL;
	container_fsconfig_t *fsc = NULL;

	if (cc == NULL) {
		return -1;
	}

	// Delayed mount operation
	fsc = &cc->fsconfig;
	// Purge runtime list
	dl_list_init(&fsc->delayed.runtime_list);

	dl_list_for_each(dmelem, &fsc->delayed.initial_list, container_delayed_mount_elem_t, list) {
		dl_list_init(&dmelem->runtime_list);
		dl_list_add_tail(&fsc->delayed.runtime_list, &dmelem->runtime_list);
	}

	return 0;
}
/**
 * Do per container delayed operation.
 * This function evaluate delayed operation trigger and do delayed operation.
 *
 * @param [in]	cc	Pointer to container_config_t.
 * @return int
 * @retval  0 Success.
 * @retval -1 Internal error.
 * @retval -2 Not run on guest.
 */
static int container_do_delayed_operation(container_config_t *cc)
{
	container_fsconfig_t *fsc = NULL;

	if (cc == NULL) {
		return -1;
	}

	if (cc->runtime_stat.status != CONTAINER_STARTED) {
		return -2;
	}

	// Delayed mount operation
	fsc = &cc->fsconfig;

	if (!dl_list_empty(&fsc->delayed.runtime_list)) {
		// When list has element.
		container_delayed_mount_elem_t *dmelem = NULL, *dmelem_n = NULL;

		dl_list_for_each_safe(dmelem, dmelem_n, &fsc->delayed.runtime_list, container_delayed_mount_elem_t, runtime_list) {
			if (dmelem->type == FSMOUNT_TYPE_DELAYED) {
				int ret = -1;

				ret = node_check(dmelem->from);
				if (ret == 0) {
					// Find node.
					ret = lxcutil_dynamic_mount_to_guest(cc, dmelem->from, dmelem->to);
					if (ret == 0) {
						// Success to delayed bind mount. Remove from list.
						// A runtime_list role is operation queue, shall not free element memory.
						dl_list_del(&dmelem->runtime_list);
						dl_list_init(&dmelem->runtime_list);
						#ifdef _PRINTF_DEBUG_
						(void) fprintf(stdout,"Delayed bind mount from %s to %s at %s\n", dmelem->from, dmelem->to, cc->name);
						#endif
					}
				}
			} else {
				// Delayed mount is only to support FSMOUNT_TYPE_DELAYED.
				dl_list_del(&dmelem->runtime_list);
				dl_list_init(&dmelem->runtime_list);
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"Got a abnormal element in delayed.runtime_list. %s\n", dmelem->from);
				#endif
			}
		}
	}

	return 0;
}
/**
 * Cleanup for per container delayed operation.
 *
 * @param [in]	cc	Pointer to container_config_t.
 * @return int
 * @retval  0 Success.
 * @retval -1 Internal error.
 * @retval -2 Syscall error.
 */
static int container_cleanup_delayed_operation(container_config_t *cc)
{
	container_fsconfig_t *fsc = NULL;

	if (cc == NULL) {
		return -1;
	}

	// Delayed mount operation
	fsc = &cc->fsconfig;
	// Purge runtime list
	dl_list_init(&fsc->delayed.runtime_list);

	return 0;
}