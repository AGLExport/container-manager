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
#include <linux/magic.h>

#include "cm-utils.h"
#include "lxc-util.h"
#include "container-config.h"
#include "device-control.h"

static int container_start_preprocess_base(container_baseconfig_t *bc);

dynamic_device_elem_data_t *dynamic_device_elem_data_create(const char *devpath, const char *devtype, const char *subsystem, const char *devnode,
															dev_t devnum, const char *diskseq, const char *partn);
int dynamic_device_elem_data_free(dynamic_device_elem_data_t *dded);

/**
 * Container start up
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
int container_device_update_guest(container_config_t *cc, dynamic_device_manager_t *ddm)
{
	int ret = 1;
	block_device_manager_t *blockdev = NULL;
	container_dynamic_device_t *cdd = NULL;
	dynamic_device_info_t *ddi = NULL, *ddi_n = NULL;
	container_dynamic_device_elem_t *cdde = NULL, *cdde_n = NULL;
	dynamic_device_elem_data_t *dded = NULL, *dded_n = NULL;
	int cmp_devpath = 0, cmp_subsystem = 0, cmp_devtype = 0;

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr, "container_device_update_guest : %s\n", cc->name);
	#endif

	if (cc->runtime_stat.status == CONTAINER_NOT_STARTED) {
		// Not starting container, pending
		return 0;
	}

	ret = dynamic_block_device_info_get(&blockdev, ddm);
	if (ret < 0) {
		// Not running dynamic device manager, pending
		return 0;
	}

	cdd = &cc->deviceconfig.dynamic_device;

	// status clean
	dl_list_for_each(cdde, &cdd->dynamic_devlist, container_dynamic_device_elem_t, list) {
		dl_list_for_each(dded, &cdde->device_list, dynamic_device_elem_data_t, list) {
			dded->is_available = 0;
		}
	}

	// check device
	dl_list_for_each(ddi, &blockdev->list, dynamic_device_info_t, list) {
		//HACK avoid ext4 fs disk/part
		if (ddi->fsmagic == EXT4_SUPER_MAGIC)
			continue;

		dl_list_for_each(cdde, &cdd->dynamic_devlist, container_dynamic_device_elem_t, list) {

			if (ddi->devpath == NULL || ddi->subsystem == NULL || ddi->devtype == NULL
				|| cdde->devtype == NULL || cdde->subsystem == NULL || cdde->devpath == NULL)
				continue;

			cmp_devtype = strcmp(cdde->devtype, ddi->devtype);
			if (cmp_devtype == 0) {
				cmp_devpath = strncmp(cdde->devpath, ddi->devpath, strlen(cdde->devpath));
				if (cmp_devpath == 0) {
					if (strcmp(cdde->subsystem, ddi->subsystem) == 0) {
						int found = 0;

						dl_list_for_each(dded, &cdde->device_list, dynamic_device_elem_data_t, list) {
							if (dded->devnum == ddi->devnum) {
								dded->is_available = 1;
								found = 1;
							}
						}

						if (found == 0) {
							// new device
							dynamic_device_elem_data_t *dded_new = NULL;
							dded_new = dynamic_device_elem_data_create(ddi->devpath, ddi->devtype, ddi->subsystem, ddi->devnode
																		, ddi->devnum, ddi->diskseq, ddi->partn);

							dded_new->is_available = 1;

							//add device to guest container
							ret = lxcutil_dynamic_device_add_to_guest(cc, dded_new, cdde->mode);
							if (ret < 0) {
								// fail to add - not register device into internal device list, retry in next event time.
								dynamic_device_elem_data_free(dded_new);
							} else {
								// success to add
								dl_list_add(&cdde->device_list, &dded_new->list);

								#ifdef _PRINTF_DEBUG_
								fprintf(stderr, "device update add %s to %s\n", dded_new->devpath, cc->name);
								#endif
							}
						}
					}
				}
			}
		}
	}

	// remove device
	dl_list_for_each(cdde, &cdd->dynamic_devlist, container_dynamic_device_elem_t, list) {
		dl_list_for_each_safe(dded, dded_n, &cdde->device_list, dynamic_device_elem_data_t, list) {
			if (dded->is_available == 0)
			{
				//remove device from guest container
				ret = lxcutil_dynamic_device_remove_from_guest(cc, dded, cdde->mode);
				if (ret < 0) {
					//  fail to remove - not remove device from internal device list, retry in next event time.
					;
				} else {
					// success to remove
					dl_list_del(&dded->list);
					dynamic_device_elem_data_free(dded);
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr, "device update del %s from %s\n", dded->devpath, cc->name);
					#endif
				}
			}
		}
	}

	return 0;

err_ret:
	
	return -1;
}
/**
 * Container start up
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
int container_device_updated(containers_t *cs)
{
	int num;
	int ret = 1;
	int result = -1;
	container_config_t *cc = NULL;

	num = cs->num_of_container;

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr, "container_device_updated exec\n");
	#endif
	
	for(int i=0;i < num;i++) {
		cc = cs->containers[i];
		ret = container_device_update_guest(cc, cs->ddm);
		if (ret < 0)
			goto err_ret;
	}

	return 0;

err_ret:
	
	return result;
}
/**
 * Container start up
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
int container_netif_update_guest(container_config_t *cc, dynamic_device_manager_t *ddm)
{
	int ret = -1;
	network_interface_manager_t *netif = NULL;
	container_dynamic_netif_t *cdn = NULL;
	network_interface_info_t *nii = NULL;
	container_dynamic_netif_elem_t *cdne = NULL;
	dynamic_device_info_t *ddi = NULL;

	if (cc->runtime_stat.status == CONTAINER_NOT_STARTED) {
		// Not starting container, pending
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
			if (cdne->ifindex == 0 && strncmp(cdne->ifname, nii->ifname, sizeof(cdne->ifname)) == 0) {
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

err_ret:
	return -1;
}
/**
 * Container start up
 *
 * @param [in]	cs	Preconstructed containers_t
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
 * Container start up
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
int container_exited(containers_t *cs, container_mngsm_guest_exit_data_t *data)
{
	int num = 0, container_num = 0;
	int ret = 1;
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
			// not runninng, not need shutdown
			;
		} else if (cc->runtime_stat.status == CONTAINER_STARTED) {
			// now runninng, guest was dead
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
	
		} else if (cc->runtime_stat.status == CONTAINER_SHUTDOWN) {
			// exit containr after shutdown request.
			cc->runtime_stat.status = CONTAINER_EXIT;
			;
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
	
	return result;
}

/**
 * Container start up
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
static int container_request_shutdown(container_config_t *cc, int sys_state)
{
	int num = 0, container_num = 0;
	int ret = -1;
	int result = 0;

	if (sys_state == CM_SYSTEM_STATE_RUN) {
		if (cc->runtime_stat.status == CONTAINER_NOT_STARTED) {
			// not runninng, not need shutdown
			;
		} else if (cc->runtime_stat.status == CONTAINER_STARTED) {
			// now runninng, send shutdown request
			ret = lxcutil_container_shutdown(cc);
			if (ret < 0) {
				//In fail case, fource kill.
				(void) lxcutil_container_fourcekill(cc);
				cc->runtime_stat.status = CONTAINER_NOT_STARTED; // guest is fource dead
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				fprintf(stderr,"[CM CRITICAL ERROR] container_request_shutdown fourcekill to %s.\n", cc->name);
				#endif
			} else {
				//Requested, wait to exit.
				cc->runtime_stat.status = CONTAINER_SHUTDOWN;
				// TODO set timeout
			}			
		} else if (cc->runtime_stat.status == CONTAINER_SHUTDOWN) {
			// Already requested shutdown, not need new action.
			;
		} else if (cc->runtime_stat.status == CONTAINER_DEAD) {
			// Already dead container, disable to re-launch.
			cc->runtime_stat.status = CONTAINER_NOT_STARTED;
		} else if (cc->runtime_stat.status == CONTAINER_EXIT) {
			// undefined state
			result = -1;
		} else {
			// undefined state
			result = -1;
		}
	} else if (sys_state == CM_SYSTEM_STATE_SHUTDOWN) {
		if (cc->runtime_stat.status == CONTAINER_NOT_STARTED) {
			// not runninng, change to exit state
			cc->runtime_stat.status = CONTAINER_EXIT;
		} else if (cc->runtime_stat.status == CONTAINER_STARTED) {
			// now runninng, send shutdown request
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"container_request_shutdown to %s\n", cc->name);
			#endif

			ret = lxcutil_container_shutdown(cc);
			if (ret < 0) {
				//In fail case, fource kill.
				(void) lxcutil_container_fourcekill(cc);
				cc->runtime_stat.status = CONTAINER_EXIT; // guest is fource exit
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				fprintf(stderr,"[CM CRITICAL ERROR] container_request_shutdown fourcekill to %s.\n", cc->name);
				#endif
			} else {
				//Requested, wait to exit.
				cc->runtime_stat.status = CONTAINER_SHUTDOWN;
				// TODO set timeout
			}			
		} else if (cc->runtime_stat.status == CONTAINER_SHUTDOWN) {
			// Already requested shutdown, not need new action.
			;
		} else if (cc->runtime_stat.status == CONTAINER_DEAD) {
			// Already dead container, it's already exit.
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
	
	return result;
}

/**
 * Container start up
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
int container_manager_shutdown(containers_t *cs)
{
	int num = 0;
	int fail_count = 0;
	int ret = -1;
	container_config_t *cc = NULL;

	cs->sys_state = CM_SYSTEM_STATE_SHUTDOWN; // chage to shutdown state

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
 * Container start up
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
int container_exec_internal_event(containers_t *cs)
{
	int num = 0;
	int fail_count = 0;
	int ret = -1;
	container_config_t *cc = NULL;

	num = cs->num_of_container;

	if (cs->sys_state == CM_SYSTEM_STATE_RUN) {
		// internal event for run state

		// Check need to auto relaunch
		for(int i=0;i < num;i++) {
			cc = cs->containers[i];
			if (cc->runtime_stat.status == CONTAINER_DEAD) {
				// Dead state -> relaunch

				//TODO relaunch
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				fprintf(stderr,"[CM CRITICAL ERROR] container_exec_internal_event need to relaunch %s.\n", cc->name);
				#endif
			}
		}

		// Check to all container shutdown timeout.
		for(int i=0;i < num;i++) {
			cc = cs->containers[i];
			// TODO timeout 
		}

	} else if (cs->sys_state == CM_SYSTEM_STATE_SHUTDOWN) {
		// internal event for shutdown state
		int exit_count = 0;

		// Check to all container was exited.
		for(int i=0;i < num;i++) {
			cc = cs->containers[i];
			if (cc->runtime_stat.status == CONTAINER_EXIT) {
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
			// TODO timeout 
		}
	} else {
		// undefined state
		return -1;
	}

out:
	return 0;
}
/**
 * Container start up
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
int container_start(containers_t *cs)
{
	int num;
	int ret = 1;
	bool bret = false;
	int result = -1;
	container_config_t *cc = NULL;
	container_control_interface_t *cci = NULL;
	
	container_mngsm_interface_get(&cci, cs);
	
	num = cs->num_of_container;

	for(int i=0;i < num;i++) {
		cc = cs->containers[i];

		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "container_start %s\n", cc->name);
		#endif
		// run preprocess 
		ret = container_start_preprocess_base(&cc->baseconfig);
		if (ret < 0) {
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			fprintf(stderr,"[CM CRITICAL ERROR] container_start: container_start_preprocess_base ret = %d\n", ret);
			#endif
			continue;
		}

		// create container inctance
		ret = lxcutil_create_instance(cc);
		if (ret < 0) {
			cc->runtime_stat.status = CONTAINER_DEAD;

			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			fprintf(stderr,"[CM CRITICAL ERROR] container_start: lxcutil_create_instance ret = %d\n", ret);
			#endif
			continue;
		}

		// TODO Need to move timing
		// Start container
		bret = cc->runtime_stat.lxc->start(cc->runtime_stat.lxc, 0, NULL);
		if (bret == false) {
			(void) lxcutil_release_instance(cc);
			cc->runtime_stat.status = CONTAINER_DEAD;

			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			fprintf(stderr,"[CM CRITICAL ERROR] container_start: lxc-start fail.\n");
			#endif
			continue;
		}

		//fprintf(stderr, "Container state: %s\n", cc->runtime_stat.lxc->state(cc->runtime_stat.lxc));
		//fprintf(stderr, "Container PID: %d\n", cc->runtime_stat.lxc->init_pid(cc->runtime_stat.lxc));
		
		ret = container_monitor_addguest(cs, cc);
		if (ret < 0) {
			// Can run guest with out monitor, critical log only.
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			fprintf(stderr,"[CM CRITICAL ERROR] container_start: container_monitor_addguest ret = %d\n", ret);
			#endif
		}

		cc->runtime_stat.status = CONTAINER_STARTED;

	}

	// dynamic device update
	cci->device_updated(cci);
	cci->netif_updated(cci);

	return 0;

err_ret:
	
	return result;
}
/**
 * Container start up
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Critical error.
 */
int container_terminate(containers_t *cs)
{
	int num;
	bool bret = false;
	container_config_t *cc = NULL;

	num = cs->num_of_container;

	for(int i=0;i < num;i++) {
		cc = cs->containers[i];

		// TODO Need to move timing
		// Stop container
		/*
		if (cc->runtime_stat.lxc != NULL) {
			bret = cc->runtime_stat.lxc->stop(cc->runtime_stat.lxc);
			if (bret == false) {
				#ifdef _PRINTF_DEBUG_
				fprintf(stderr, "[FAIL] lxcutil_lxc-start\n");
				#endif
				;
			}
		}*/

		(void)lxcutil_release_instance(cc);
	}

	return 0;
}

/**
 * Disk mount procedure for failover
 *
 * @param [in]	devs	Array of disk block device. A and B.
 * @param [in]	path	Mount path.
 * @param [in]	fstype	Name of file system. When fstype == NULL, file system is auto.
 * @param [in]	mntflag	Mount flag.
 * @return int
 * @retval  1 Success - secoundary.
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
				ret = umount2(path, MNT_FORCE);
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
				break;
			} else {
				//error - try to mount secoundary disk
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

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr,"container_start_mountdisk_failover: %s mount to %s (%s)\n", dev, path, fstype);
	#endif

	return mntdisk;
}
/**
 * Disk mount procedure for a/b
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
			ret = umount2(path, MNT_FORCE);
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
	fprintf(stderr,"container_start_mountdisk_ab: %s mount to %s (%s)\n", dev, path, fstype);
	#endif

	return 0;
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
static int container_start_preprocess_base(container_baseconfig_t *bc)
{
	int ret = 1;
	int result = -1;
	int abboot = 0;
	const char *dev = NULL, *path = NULL, *fstyp = NULL; 
	unsigned long mntflag = 0;

	// mount rootfs
	if (bc->rootfs.mode == DISKMOUNT_TYPE_RW) {
		mntflag = MS_DIRSYNC | MS_NOATIME | MS_NODEV | MS_NOEXEC | MS_SYNCHRONOUS;
	} else {
		mntflag = MS_NOATIME | MS_RDONLY;
	}

	ret = container_start_mountdisk_ab(bc->rootfs.blockdev, bc->rootfs.path
										, bc->rootfs.filesystem, mntflag, bc->abboot);
	if ( ret < 0) {
		// root fs mount is Mandatry.
		#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
		fprintf(stderr,"[CM CRITICAL ERROR] container_start_preprocess_base: mandatry disk %s could not mount\n", bc->rootfs.blockdev[bc->abboot]);
		#endif
		return -1;
	}

	// mount extradisk - optional
	if (!dl_list_empty(&bc->extradisk_list)) {
		int extdiskmnt = 0;
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
					// AB disk mout is mandatry function.
					#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
					fprintf(stderr,"[CM CRITICAL ERROR] container_start_preprocess_base: mandatry disk %s could not mount\n", exdisk->blockdev[bc->abboot]);
					#endif
					return -1;
				}
			} else {
				ret = container_start_mountdisk_failover(exdisk->blockdev, exdisk->from, exdisk->filesystem, mntflag);
				if (ret < 0) {
					// Failover disk mout is optional function.
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
 * Preprocess for container start base
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 mount error.
 * @retval -2 Syscall error. 
 */
dynamic_device_elem_data_t *dynamic_device_elem_data_create(const char *devpath, const char *devtype, const char *subsystem, const char *devnode,
															dev_t devnum, const char *diskseq, const char *partn)
{
	dynamic_device_elem_data_t *dded = NULL;

	dded = (dynamic_device_elem_data_t*)malloc(sizeof(dynamic_device_elem_data_t));
	if (dded == NULL)
		return NULL;

	memset(dded, 0 ,sizeof(dynamic_device_elem_data_t));

	dded->devpath = strdup(devpath);
	dded->devtype = strdup(devtype);
	dded->subsystem = strdup(subsystem);
	dded->devnode = strdup(devnode);
	dded->devnum = devnum;

	//options
	if (diskseq != NULL)
		dded->diskseq = strdup(diskseq);
	if (partn != NULL)
		dded->partn = strdup(partn);

	dl_list_init(&dded->list);

	return dded;
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
int dynamic_device_elem_data_free(dynamic_device_elem_data_t *dded)
{
	if (dded == NULL)
		return -1;

	free(dded->devpath);
	free(dded->devtype);
	free(dded->subsystem);
	free(dded->devnode);
	free(dded->diskseq);
	free(dded->partn);
	free(dded);

	return 0;
}

