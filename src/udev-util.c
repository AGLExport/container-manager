/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	udev-util.c
 * @brief	udev utility functions
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

struct s_udevmonitor {
	struct udev* pudev;
	struct udev_monitor *pudev_monitor;
	sd_event_source *libudev_source ;
	container_control_interface_t *cci;
};
const char dev_subsys_block[] = "block";
const char dev_subsys_net[] = "net";

static const char *block_dev_blacklist[] = {
	"/dev/mmcblk",
	"/dev/nvme",
	NULL,
};

#ifdef _PRINTF_DEBUG_
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
 * Setup for the uevent monitor event loop.
 *
 * @param [out]	handle	Pointer to variable of udevmonitor_t;
 * @param [in]	event	Instance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int udevmonitor_devevent(dynamic_device_manager_t *ddm)
{
	int ret = -1;
	struct s_udevmonitor *udevmon = NULL;
	struct udev_device *pdev = NULL;
	const char *paction = NULL, *subsys = NULL;
	dev_t devnum = 0;

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
 * device add to list with fixup.
 *
 * @param [out]	handle	Pointer to variable of udevmonitor_t;
 * @param [in]	event	Instance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int udevmonitor_devevent_add_block(dynamic_device_manager_t *ddm, dynamic_device_info_t *new_ddi)
{
	int ret = -1;
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
 * device remove from list with fixup.
 *
 * @param [out]	handle	Pointer to variable of udevmonitor_t;
 * @param [in]	event	Instance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int udevmonitor_devevent_remove_block(dynamic_device_manager_t *ddm, dynamic_device_info_t *del_ddi)
{
	int ret = -1;
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
 * device change for list with fixup.
 *
 * @param [out]	handle	Pointer to variable of udevmonitor_t;
 * @param [in]	event	Instance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int udevmonitor_devevent_change_block(dynamic_device_manager_t *ddm, dynamic_device_info_t *new_ddi)
{
	int ret = -1;
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
 * device add to list with fixup.
 *
 * @param [out]	handle	Pointer to variable of udevmonitor_t;
 * @param [in]	event	Instance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int udevmonitor_devevent_add_net(dynamic_device_manager_t *ddm, dynamic_device_info_t *new_ddi)
{
	int ret = -1;
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
 * device remove from list with fixup.
 *
 * @param [out]	handle	Pointer to variable of udevmonitor_t;
 * @param [in]	event	Instance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int udevmonitor_devevent_remove_net(dynamic_device_manager_t *ddm, dynamic_device_info_t *del_ddi)
{
	int ret = -1;
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
 * device change for list with fixup.
 *
 * @param [out]	handle	Pointer to variable of udevmonitor_t;
 * @param [in]	event	Instance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int udevmonitor_devevent_change_net(dynamic_device_manager_t *ddm, dynamic_device_info_t *new_ddi)
{
	int ret = -1;
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
 * Setup for the uevent monitor event loop.
 *
 * @param [out]	handle	Pointer to variable of udevmonitor_t;
 * @param [in]	event	Instance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
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
		dev_t devnum = 0;
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
 * @param [out]	handle	Pointer to variable of udevmonitor_t;
 * @param [in]	event	Instance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
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
 * @param [in]	handle	Handle created by udevmonitor_setup;
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
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
 * Create for the dynamic_device_info_t data.
 *
 * @param [in]	handle	Handle created by udevmonitor_setup;
 * @return int	 0 success
 * 				 1 on the blacklist
 * 				-3 argument error
 *				-2 internal error
 *				-1 Mandatory data is nothing
 */
static int dynamic_device_info_create_block(dynamic_device_info_t **ddi, struct udev_device *pdev, const char* subsys)
{
	int ret = -1;
	const char *paction = NULL, *devpath = NULL;
	const char *devnode = NULL, *syspath = NULL;
	const char *sysname = NULL, *devtype = NULL;
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
 * Create for the dynamic_device_info_t data.
 *
 * @param [in]	handle	Handle created by udevmonitor_setup;
 * @return int	 0 success
 * 				-3 argument error
 *				-2 internal error
 *				-1 Mandatory data is nothing
 */
static int dynamic_device_info_create_net(dynamic_device_info_t **ddi, struct udev_device *pdev, const char* subsys)
{
	const char *paction = NULL, *devpath = NULL;
	const char *syspath = NULL;
	const char *sysname = NULL, *devtype = NULL;
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
 *
 * @param [in]	handle	Handle created by udevmonitor_setup;
 * @return int	 0 success
 * 				-1 argument error
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
 * Sub function for uevent monitor.
 * Cleanup for the dynamic_device_info_t data.
 *
 * @param [in]	handle	Handle created by udevmonitor_setup;
 * @return int	 0 success
 * 				-1 argument error
 */
int dynamic_block_device_info_get(block_device_manager_t **blockdev, dynamic_device_manager_t *ddm)
{
	if (blockdev == NULL || ddm == NULL)
		return -1;

	(*blockdev) = &ddm->blockdev;

	return 0;
}