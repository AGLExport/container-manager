/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	udev-util.c
 * @brief	This file include device management functions using libudev for container manager.
 */
#include "udev-util.h"

#include <stdlib.h>
#include <systemd/sd-event.h>
#include <libudev.h>

#include <stdio.h>
#include <string.h>
#include <sys/sysmacros.h>

#include "block-util.h"

#undef _PRINTF_DEBUG_

/**
 * @struct	s_udevmonitor
 * @brief	The data structure for device monitor using libudev.
 */
struct s_udevmonitor {
	struct udev* pudev;					/**< The udev object created by libudev. */
	struct udev_monitor *pudev_monitor;	/**< The udev_monitor object created by libudev.  */
	sd_event_source *libudev_source ;	/**< The sd event source controlled by libudev. */
	container_control_interface_t *cci;	/**< Reference to container manager control interface. */
};

/**
 * @var		dev_subsys_block
 * @brief	Defined string to use at subsystem test. - for block device.
 */
const char dev_subsys_block[] = "block";
/**
 * @var		dev_subsys_net
 * @brief	Defined string to use at subsystem test. - for net device.
 */
const char dev_subsys_net[] = "net";

/**
 * @var		block_dev_blacklist
 * @brief	Black list for block device management.  When device name mach this list, that device is not manage by dynamic device manager.
 */
static const char *block_dev_blacklist[] = {
	"/dev/mmcblk",
	"/dev/nvme",
	NULL,
};

#ifdef _PRINTF_DEBUG_
/**
 * Debug use only.
 */
static void print_dynamic_device_info_one(dynamic_device_info_t *ddi)
{
	if (strncmp(dev_subsys_block,ddi->subsystem,sizeof(dev_subsys_block)) == 0) {
		fprintf(stderr, " syspath = %s\n sysname = %s\n devpath = %s\n devnode = %s\n devtype = %s\n subsystem = %s\n mj:mn = %d : %d\n",
						ddi->syspath, ddi->sysname, ddi->devpath, ddi->devnode,
						ddi->devtype, ddi->subsystem, major(ddi->devnum), minor(ddi->devnum));
		//option
		if (ddi->diskseq != NULL)
			fprintf(stderr, " diskseq = %s\n",ddi->diskseq);

		if (ddi->partn != NULL)
			fprintf(stderr, " partn = %s\n",ddi->partn);

		fprintf(stderr, "\n");

	} else 	if (strncmp(dev_subsys_net,ddi->subsystem,sizeof(dev_subsys_net)) == 0) {
		fprintf(stderr, " syspath = %s\n sysname = %s\n devpath = %s\n devtype = %s\n ifindex = %d\n\n",
						ddi->syspath, ddi->sysname, ddi->devpath, ddi->devtype, ddi->ifindex);

	}
}
/**
 * Debug use only.
 */
static void print_dynamic_device_info(dynamic_device_manager_t *ddm, int mode)
{
	dynamic_device_info_t *ddi = NULL;

	if ((mode & 0x1u) != 0) {
		fprintf(stderr, "ddinfo print char/block devices\n");
		dl_list_for_each(ddi, &ddm->blockdev.list, dynamic_device_info_t, list) {
			print_dynamic_device_info_one(ddi);
		}
	}

	if ((mode & 0x2u) != 0) {
		fprintf(stderr, "ddinfo print new devices\n");
		dl_list_for_each(ddi, &ddm->netif.devlist, dynamic_device_info_t, list) {
			print_dynamic_device_info_one(ddi);
		}
	}
}
#endif //#ifdef _PRINTF_DEBUG_

static int dynamic_device_info_free(dynamic_device_info_t *ddi);
static int dynamic_device_info_create_block(dynamic_device_info_t **ddi, struct udev_device *pdev, const char* subsys);
static int dynamic_device_info_create_net(dynamic_device_info_t **ddi, struct udev_device *pdev, const char* subsys);

static int udevmonitor_devevent(dynamic_device_manager_t *ddm);

static int udevmonitor_devevent_add_block(dynamic_device_manager_t *ddm, dynamic_device_info_t *new_ddi);
static int udevmonitor_devevent_remove_block(dynamic_device_manager_t *ddm, dynamic_device_info_t *del_ddi);
static int udevmonitor_devevent_change_block(dynamic_device_manager_t *ddm, dynamic_device_info_t *new_ddi);

static int udevmonitor_devevent_add_net(dynamic_device_manager_t *ddm, dynamic_device_info_t *new_ddi);
static int udevmonitor_devevent_remove_net(dynamic_device_manager_t *ddm, dynamic_device_info_t *del_ddi);
static int udevmonitor_devevent_change_net(dynamic_device_manager_t *ddm, dynamic_device_info_t *new_ddi);

/**
 * Event handler for libudev.
 * This function analyze received data using udev_monitor by libudev.
 *
 * @param [in]	event		libudev event source object.
 * @param [in]	fd			File descriptor for udev_monitor.
 * @param [in]	revents		Active event (epoll).
 * @param [in]	userdata	Pointer to dynamic_device_manager_t.
 * @return int
 * @retval	0	Success to event handling.
 * @retval	-1	Internal error (Not use).
 */
static int udev_event_handler(sd_event_source *event, int fd, uint32_t revents, void *userdata)
{
	int ret = 0;
	dynamic_device_manager_t *ddm = NULL;

	if (userdata == NULL) {
		// Fail safe - disable udev event
		sd_event_source_disable_unref(event);
		return 0;
	}

	ddm = (dynamic_device_manager_t*)userdata;

	if ((revents & (EPOLLHUP | EPOLLERR)) != 0) {
		// Fail safe - disable udev event
		sd_event_source_disable_unref(event);
	} else if ((revents & EPOLLIN) != 0) {
		// Receive
		ret = udevmonitor_devevent(ddm);
	}

	return ret;
}

/**
 * Sub function for uevent monitor.
 * This function analyze supported device type (block, net) and create device information object (dynamic_device_info_t).
 *
 * @param [in]	ddm	Pointer to dynamic_device_manager_t.
 * @return int
 * @retval	0	Success to get device info.
 * @retval	-1	Internal error.
 * @retval	-2	Argument error. (Reserve)
 */
static int udevmonitor_devevent(dynamic_device_manager_t *ddm)
{
	int ret = -1;
	struct s_udevmonitor *udevmon = NULL;
	struct udev_device *pdev = NULL;
	const char *paction = NULL, *subsys = NULL;

	udevmon = (struct s_udevmonitor*)ddm->udevmon;
	pdev = udev_monitor_receive_device(udevmon->pudev_monitor);
	if (pdev == NULL)
		goto error_ret;

	subsys = udev_device_get_subsystem(pdev);
	if (subsys == NULL)
		goto error_ret;

	if (strncmp(dev_subsys_block, subsys, sizeof(dev_subsys_block)) == 0) {
		dynamic_device_info_t *ddinfo = NULL;
		container_control_interface_t *cci = NULL;;

		// block subsystem update
		ret = dynamic_device_info_create_block(&ddinfo, pdev, subsys);
		if (ret == 1)
			goto bypass_ret;
		else if (ret < 0)
			goto error_ret;

		// dispatch event
		paction = udev_device_get_action(pdev);
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"blockdev %s \n", paction);
		print_dynamic_device_info_one(ddinfo);
		#endif

		if (strncmp(paction, "add", strlen(paction)) == 0) {
			(void)udevmonitor_devevent_add_block(ddm, ddinfo);
			// new ddinfo into list, shall not be free
		} else if (strncmp(paction, "remove", strlen(paction)) == 0) {
			(void)udevmonitor_devevent_remove_block(ddm, ddinfo);
			// new ddinfo is only to use remove device scanning, shall be free
			dynamic_device_info_free(ddinfo);
		} else if (strncmp(paction, "change", strlen(paction)) == 0) {
			(void)udevmonitor_devevent_change_block(ddm, ddinfo);
		}

		// Update notification
		cci = udevmon->cci;
		cci->device_updated(cci);

	} else if (strncmp(dev_subsys_net, subsys, sizeof(dev_subsys_net)) == 0) {
		dynamic_device_info_t *ddinfo = NULL;
		// net subsystem update

		ret = dynamic_device_info_create_net(&ddinfo, pdev, subsys);
		if (ret < 0)
			return -1;

		// dispatch event
		paction = udev_device_get_action(pdev);
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"netif %s \n", paction);
		print_dynamic_device_info_one(ddinfo);
		#endif

		if (strncmp(paction, "add", strlen(paction)) == 0) {
			(void)udevmonitor_devevent_add_net(ddm, ddinfo);
			// new ddinfo into list, shall not be free
		} else if (strncmp(paction, "remove", strlen(paction)) == 0) {
			(void)udevmonitor_devevent_remove_net(ddm, ddinfo);
			// new ddinfo is only to use remove device scanning, shall be free
			dynamic_device_info_free(ddinfo);
		} else if (strncmp(paction, "move", strlen(paction)) == 0) {
			(void)udevmonitor_devevent_change_net(ddm, ddinfo);
		}

	}

bypass_ret:
	udev_device_unref(pdev);

	return 0;

error_ret:
	if (pdev != NULL)
		udev_device_unref(pdev);

	return -1;
}
/**
 * Sub function for uevent monitor.
 * This function add block device infomation to list. It supports list fixup.
 *
 * @param [in]	ddm		Pointer to dynamic_device_manager_t.
 * @param [in]	new_ddi	Pointer to created new dynamic_device_info_t.
 * @return int
 * @retval	0	Success to add device infomation to list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int udevmonitor_devevent_add_block(dynamic_device_manager_t *ddm, dynamic_device_info_t *new_ddi)
{
	dynamic_device_info_t *ddi = NULL, *ddi_n = NULL;

	if (ddm == NULL || new_ddi == NULL)
		return -2;

	dl_list_for_each_safe(ddi, ddi_n, &ddm->blockdev.list, dynamic_device_info_t, list) {

		if (ddi->devnum == new_ddi->devnum) {
			// existing device is found -> remove
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"udevmonitor: udevmonitor_devevent_add found existing device \n exist = %s\n new = %s\n\n"
					, ddi->devpath, new_ddi->devpath);
			#endif
			dl_list_del(&ddi->list);
			dynamic_device_info_free(ddi);
		}
	}

	dl_list_add(&ddm->blockdev.list, &new_ddi->list);

	return 0;
}
/**
 * Sub function for uevent monitor.
 * This function remove block device infomation from list.
 *
 * @param [in]	ddm		Pointer to dynamic_device_manager_t.
 * @param [in]	del_ddi	Pointer to removed device infomation (dynamic_device_info_t).
 * @return int
 * @retval	0	Success to remove device infomation from list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int udevmonitor_devevent_remove_block(dynamic_device_manager_t *ddm, dynamic_device_info_t *del_ddi)
{
	dynamic_device_info_t *ddi = NULL, *ddi_n = NULL;

	if (ddm == NULL || del_ddi == NULL)
		return -2;

	dl_list_for_each_safe(ddi, ddi_n, &ddm->blockdev.list, dynamic_device_info_t, list) {

		if (ddi->devnum == del_ddi->devnum) {
			// existing device is found -> remove
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"udevmonitor: udevmonitor_devevent_del found existing device \n exist = %s\n new = %s\n\n"
					, ddi->devpath, del_ddi->devpath);
			#endif
			dl_list_del(&ddi->list);
			dynamic_device_info_free(ddi);
		}
	}

	return 0;
}
/**
 * Sub function for uevent monitor.
 * This function change block device infomation to list.  That function exchange old ddi to new ddi.
 *
 * @param [in]	ddm		Pointer to dynamic_device_manager_t.
 * @param [in]	new_ddi	Pointer to created new dynamic_device_info_t.
 * @return int
 * @retval	0	Success to change device infomation at list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int udevmonitor_devevent_change_block(dynamic_device_manager_t *ddm, dynamic_device_info_t *new_ddi)
{
	dynamic_device_info_t *ddi = NULL, *ddi_n = NULL;

	if (ddm == NULL || new_ddi == NULL)
		return -2;

	dl_list_for_each_safe(ddi, ddi_n, &ddm->blockdev.list, dynamic_device_info_t, list) {

		if (ddi->devnum == new_ddi->devnum) {
			// existing device is found -> remove
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"udevmonitor: udevmonitor_devevent_del found existing device \n exist = %s\n new = %s\n\n"
					, ddi->devpath, new_ddi->devpath);
			#endif
			dl_list_del(&ddi->list);
			dynamic_device_info_free(ddi);
		}
	}

	dl_list_add(&ddm->blockdev.list, &new_ddi->list);

	return 0;
}
/**
 * Sub function for uevent monitor.
 * This function add net device infomation to list. It supports list fixup.
 *
 * @param [in]	ddm		Pointer to dynamic_device_manager_t.
 * @param [in]	new_ddi	Pointer to created new dynamic_device_info_t.
 * @return int
 * @retval	0	Success to add device infomation to list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int udevmonitor_devevent_add_net(dynamic_device_manager_t *ddm, dynamic_device_info_t *new_ddi)
{
	dynamic_device_info_t *ddi = NULL, *ddi_n = NULL;

	if (ddm == NULL || new_ddi == NULL)
		return -2;

	dl_list_for_each_safe(ddi, ddi_n, &ddm->netif.devlist, dynamic_device_info_t, list) {

		if (ddi->ifindex == new_ddi->ifindex) {
			// existing device is found -> remove
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"udevmonitor: udevmonitor_devevent_add found existing device \n exist = %s\n new = %s\n\n"
					, ddi->devpath, new_ddi->devpath);
			#endif
			dl_list_del(&ddi->list);
			dynamic_device_info_free(ddi);
		}
	}

	dl_list_add(&ddm->netif.devlist, &new_ddi->list);

	return 0;
}
/**
 * Sub function for uevent monitor.
 * This function remove net device infomation from list.
 *
 * @param [in]	ddm		Pointer to dynamic_device_manager_t.
 * @param [in]	del_ddi	Pointer to removed device infomation (dynamic_device_info_t).
 * @return int
 * @retval	0	Success to remove device infomation from list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int udevmonitor_devevent_remove_net(dynamic_device_manager_t *ddm, dynamic_device_info_t *del_ddi)
{
	dynamic_device_info_t *ddi = NULL, *ddi_n = NULL;

	if (ddm == NULL || del_ddi == NULL)
		return -2;

	dl_list_for_each_safe(ddi, ddi_n, &ddm->netif.devlist, dynamic_device_info_t, list) {

		// In del timing, ifindex is lost. need to use ifname(=sysname).
		if (strcmp(ddi->sysname, del_ddi->sysname) == 0) {
			// existing device is found -> remove
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"udevmonitor: udevmonitor_devevent_del found existing device \n exist = %s\n new = %s\n\n"
					, ddi->devpath, del_ddi->devpath);
			#endif
			dl_list_del(&ddi->list);
			dynamic_device_info_free(ddi);
		}
	}

	return 0;
}
/**
 * Sub function for uevent monitor.
 * This function change net device infomation to list.  That function exchange old ddi to new ddi.
 *
 * @param [in]	ddm		Pointer to dynamic_device_manager_t.
 * @param [in]	new_ddi	Pointer to created new dynamic_device_info_t.
 * @return int
 * @retval	0	Success to change device infomation at list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int udevmonitor_devevent_change_net(dynamic_device_manager_t *ddm, dynamic_device_info_t *new_ddi)
{
	dynamic_device_info_t *ddi = NULL, *ddi_n = NULL;

	if (ddm == NULL || new_ddi == NULL)
		return -2;

	dl_list_for_each_safe(ddi, ddi_n, &ddm->netif.devlist, dynamic_device_info_t, list) {

		if (ddi->ifindex == new_ddi->ifindex) {
			// existing device is found -> remove
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"udevmonitor: udevmonitor_devevent_del found existing device \n exist = %s\n new = %s\n\n"
					, ddi->devpath, new_ddi->devpath);
			#endif
			dl_list_del(&ddi->list);
			dynamic_device_info_free(ddi);
		}
	}

	dl_list_add(&ddm->netif.devlist, &new_ddi->list);

	return 0;
}
/**
 * Sub function for uevent monitor.
 * This function create internal management data in initialization time.  That work aim to sync current device status.
 * This function shall call immediately after udev_monitor enabled.
 *
 * @param [in]	ddm	Pointer to dynamic_device_manager_t.
 * @return int
 * @retval	0	Success to change device infomation at list.
 * @retval	-1	Internal error.
 * @retval	-2	Argument error.
 */
static int udevmonitor_scan(dynamic_device_manager_t *ddm)
{
	int ret = -1;
	struct s_udevmonitor *udevmon = NULL;
	struct udev* pudev = NULL;
	struct udev_device *pdev = NULL;
	struct udev_enumerate *penum = NULL;
	struct udev_list_entry *devices = NULL, *dev_list_entry = NULL;

	udevmon = (struct s_udevmonitor*)ddm->udevmon;
	pudev = udevmon->pudev;
	if (pudev == NULL)
		return -2;

	/* create enumerate object */
	penum = udev_enumerate_new(pudev);
	if (penum == NULL)
		return -1;

	udev_enumerate_add_match_subsystem(penum, dev_subsys_block);
	udev_enumerate_add_match_subsystem(penum, "net");
	udev_enumerate_scan_devices(penum);

	devices = udev_enumerate_get_list_entry(penum);
	if (devices == NULL) {
		udev_enumerate_unref(penum);
		return -1;
	}

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path = NULL, *subsys = NULL;
		dynamic_device_info_t *ddinfo = NULL;

		path = udev_list_entry_get_name(dev_list_entry);
		if (path == NULL)
			continue;

		pdev = udev_device_new_from_syspath(pudev, path);
		if (pdev == NULL)
			continue;

		subsys = udev_device_get_subsystem(pdev);
		if (subsys == NULL) {
			udev_device_unref(pdev);
			continue;
		}

		if (strncmp(dev_subsys_block, subsys, sizeof(dev_subsys_block)) == 0) {

			ret = dynamic_device_info_create_block(&ddinfo, pdev, subsys);
			if (ret < 0 || ret == 1) {
				udev_device_unref(pdev);
				continue;
			}

			dl_list_add(&ddm->blockdev.list, &ddinfo->list);

		} else if (strncmp(dev_subsys_net, subsys, sizeof(dev_subsys_net)) == 0) {
			ret = dynamic_device_info_create_net(&ddinfo, pdev, subsys);
			if (ret < 0) {
				udev_device_unref(pdev);
				continue;
			}

			dl_list_add(&ddm->netif.devlist, &ddinfo->list);
		}

		/* free dev */
		udev_device_unref(pdev);
	}
	/* free enumerate */
	udev_enumerate_unref(penum);

	return 0;
}

/**
 * Sub function for uevent monitor.
 * Setup for the uevent monitor event loop.
 *
 * @param [in]	ddm		Pointer to dynamic_device_manager_t.
 * @param [in]	cci		Pointer to container_control_interface_t to send event notification to container manager state machine.
 * @param [in]	event	Instance of sd_event. (main loop)
 * @return int
 * @retval	0	Success to change device infomation at list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
int udevmonitor_setup(dynamic_device_manager_t *ddm, container_control_interface_t *cci, sd_event *event)
{
	struct s_udevmonitor *devmon = NULL;
	struct udev* pudev = NULL;
	struct udev_monitor *pudev_monitor = NULL;
	sd_event_source *libudev_source = NULL;
	int fd = -1;
	int ret = -1;

	if (ddm == NULL || cci == NULL || event == NULL)
		return -2;

	devmon = malloc(sizeof(struct s_udevmonitor));
	if (devmon == NULL)
		goto err_return;

	memset(devmon,0,sizeof(struct s_udevmonitor));

	pudev = udev_new();
	if (pudev == NULL)
		goto err_return;

	pudev_monitor = udev_monitor_new_from_netlink(pudev,"kernel");
	if (pudev_monitor == NULL)
		goto err_return;

	ret = udev_monitor_enable_receiving(pudev_monitor);
	if (ret < 0)
		goto err_return;

	fd = udev_monitor_get_fd(pudev_monitor);
	if (fd < 0)
		goto err_return;

	ret = sd_event_add_io(event, &libudev_source, fd, EPOLLIN, udev_event_handler, ddm);
	if (ret < 0)
		goto err_return;

	devmon->pudev = pudev;
	devmon->pudev_monitor = pudev_monitor;
	devmon->libudev_source = libudev_source;
	devmon->cci = cci;

	dl_list_init(&ddm->blockdev.list);
	dl_list_init(&ddm->netif.devlist);
	ddm->udevmon = (udevmonitor_t*)devmon;

	udevmonitor_scan(ddm);

	return 0;

err_return:
	if (pudev_monitor != NULL)
		udev_monitor_unref(pudev_monitor);

	if (pudev != NULL)
		udev_unref(pudev);

	if (devmon != NULL)
		free(devmon);

	ddm->udevmon = NULL;

	return -1;
}
/**
 * Sub function for uevent monitor.
 * Cleanup for the uevent monitor event loop.
 *
 * @param [in]	ddm		Pointer to dynamic_device_manager_t.
 * @return int
 * @retval	0	Success to change device infomation at list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
int udevmonitor_cleanup(dynamic_device_manager_t *ddm)
{
	struct s_udevmonitor *devmon = NULL;

	if (ddm == NULL)
		return -2;

	{
		dynamic_device_info_t *ddi = NULL, *ddi_n = NULL;

		dl_list_for_each_safe(ddi, ddi_n, &ddm->blockdev.list, dynamic_device_info_t, list) {
			dl_list_del(&ddi->list);
			dynamic_device_info_free(ddi);
		}
	}

	{
		dynamic_device_info_t *ddi = NULL, *ddi_n = NULL;

		dl_list_for_each_safe(ddi, ddi_n, &ddm->netif.devlist, dynamic_device_info_t, list) {
			dl_list_del(&ddi->list);
			dynamic_device_info_free(ddi);
		}
	}

	devmon = (struct s_udevmonitor*)ddm->udevmon;

	if (devmon->libudev_source != NULL)
		(void)sd_event_source_disable_unref(devmon->libudev_source);

	if (devmon->pudev_monitor != NULL)
		udev_monitor_unref(devmon->pudev_monitor);

	if (devmon->pudev != NULL)
		udev_unref(devmon->pudev);

	free(devmon);

	return 0;

}
/**
 * Sub function for uevent monitor.
 * Create dynamic_device_info_t data that support block device.
 *
 * @param [out]	ddi		Double pointer to get created dynamic_device_info_t object.
 * @param [in]	pdev	Pointer to udev_device.
 * @param [in]	subsys	A name of device subsystem. (string)
 * @return int
 * @retval	0	Success to create dynamic_device_info_t.
 * @retval	1	Device found in blacklist.
 * @retval	-1	Mandatory data is nothing in udev_device.
 * @retval	-2	Internal error.
 * @retval	-3	Argument error.
 */
static int dynamic_device_info_create_block(dynamic_device_info_t **ddi, struct udev_device *pdev, const char* subsys)
{
	int ret = -1;
	const char *pstr = NULL;
	dev_t devnum = 0;
	dynamic_device_info_t *ddinfo = NULL;
	block_device_info_t bdi;

	if (ddi == NULL || pdev == NULL || subsys == NULL)
		return -3;

	pstr = udev_device_get_devnode(pdev);
	devnum = udev_device_get_devnum(pdev);
	if (pstr == NULL || devnum == 0) {
		//Mandatory data is nothing
		return -1;
	}

	for (int i=0; block_dev_blacklist[i] != NULL; i++) {
		ret = strncmp(pstr, block_dev_blacklist[i], strlen(block_dev_blacklist[i]));
		if (ret == 0) {
			return 1;
		}
	}

	ddinfo = (dynamic_device_info_t*)malloc(sizeof(dynamic_device_info_t));
	if (ddinfo == NULL)
		return -2;

	memset(ddinfo, 0, sizeof(dynamic_device_info_t));
	memset(&bdi, 0, sizeof(bdi));

	ddinfo->devnum = devnum;
	ddinfo->devnode = strdup(pstr);
	ddinfo->subsystem = strdup(subsys);

	pstr = udev_device_get_syspath(pdev);
	if (pstr != NULL)
		ddinfo->syspath = strdup(pstr);

	pstr = udev_device_get_sysname(pdev);
	if (pstr != NULL)
		ddinfo->sysname = strdup(pstr);

	pstr = udev_device_get_devpath(pdev);
	if (pstr != NULL)
		ddinfo->devpath = strdup(pstr);

	pstr = udev_device_get_devtype(pdev);
	if (pstr != NULL)
		ddinfo->devtype = strdup(pstr);

	pstr = udev_device_get_property_value(pdev, "DISKSEQ");
	if (pstr != NULL)
		ddinfo->diskseq = strdup(pstr);

	pstr = udev_device_get_property_value(pdev, "PARTN");
	if (pstr != NULL)
		ddinfo->partn = strdup(pstr);

	ret = block_util_getfs(ddinfo->devnode, &bdi);
	if (ret == 0)
		ddinfo->fsmagic = bdi.fsmagic;

	dl_list_init(&ddinfo->list);

	(*ddi) = ddinfo;

	return 0;
}
/**
 * Sub function for uevent monitor.
 * Create dynamic_device_info_t data that support net device.
 *
 * @param [out]	ddi		Double pointer to get created dynamic_device_info_t object.
 * @param [in]	pdev	Pointer to udev_device.
 * @param [in]	subsys	A name of device subsystem. (string)
 * @return int
 * @retval	0	Success to create dynamic_device_info_t.
 * @retval	1	Device found in blacklist.
 * @retval	-1	Mandatory data is nothing in udev_device.
 * @retval	-2	Internal error.
 * @retval	-3	Argument error.
 */
static int dynamic_device_info_create_net(dynamic_device_info_t **ddi, struct udev_device *pdev, const char* subsys)
{
	int ifindex = -1;
	const char *pstr = NULL;
	char *endptr = NULL;;
	dynamic_device_info_t *ddinfo = NULL;

	if (ddi == NULL || pdev == NULL || subsys == NULL)
		return -3;

	pstr = udev_device_get_sysattr_value(pdev, "ifindex");
	if (pstr != NULL) {
		ifindex = strtol(pstr, &endptr, 10);
		if (pstr == endptr) {
			//Can't convert str to long, mandatory data is nothing
			return -1;
		}
	}

	ddinfo = (dynamic_device_info_t*)malloc(sizeof(dynamic_device_info_t));
	if (ddinfo == NULL)
		return -2;

	memset(ddinfo, 0, sizeof(dynamic_device_info_t));

	ddinfo->subsystem = strdup(subsys);
	ddinfo->ifindex = ifindex;

	pstr = udev_device_get_syspath(pdev);
	if (pstr != NULL)
		ddinfo->syspath = strdup(pstr);

	pstr = udev_device_get_sysname(pdev);
	if (pstr != NULL)
		ddinfo->sysname = strdup(pstr);

	pstr = udev_device_get_devpath(pdev);
	if (pstr != NULL)
		ddinfo->devpath = strdup(pstr);

	pstr = udev_device_get_devtype(pdev);
	if (pstr != NULL)
		ddinfo->devtype = strdup(pstr);

	dl_list_init(&ddinfo->list);

	(*ddi) = ddinfo;

	return 0;
}
/**
 * Sub function for uevent monitor.
 * Cleanup for the dynamic_device_info_t data.
 * After this function call, dynamic_device_info_t object must not use.
 *
 * @param [in]	ddi	Pointer to dynamic_device_info_t.
 * @return int
 * @retval	0	Success to create dynamic_device_info_t.
 * @retval	-1	Argument error.
 */
static int dynamic_device_info_free(dynamic_device_info_t *ddi)
{
	if (ddi == NULL)
		return -1;

	free(ddi->syspath);
	ddi->syspath = NULL;

	free(ddi->sysname);
	ddi->sysname = NULL;

	free(ddi->devpath);
	ddi->devpath = NULL;

	free(ddi->devtype);
	ddi->devtype = NULL;

	free(ddi->subsystem);
	ddi->subsystem = NULL;

	free(ddi->devnode);
	ddi->devnode = NULL;

	free(ddi->diskseq);
	ddi->diskseq = NULL;

	free(ddi->partn);
	ddi->partn = NULL;

	free(ddi);

	return 0;
}
/**
 * Get block_device_manager_t object from dynamic_device_manager_t.
 * This function provide block device list access interface that is used by container management block.
 *
 * @param [out]	blockdev	Double pointer to block_device_manager_t to get reference of block_device_manager_t object.
 * @param [in]	ddm			Pointer to dynamic_device_manager_t.
 * @return int
 * @retval	0	Success to create dynamic_device_info_t.
 * @retval	-1	Argument error.
 */
int dynamic_block_device_info_get(block_device_manager_t **blockdev, dynamic_device_manager_t *ddm)
{
	if (blockdev == NULL || ddm == NULL)
		return -1;

	(*blockdev) = &ddm->blockdev;

	return 0;
}