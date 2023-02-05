/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	udev-util.c
 * @brief	This file include device management functions using libudev for container manager.
 */
#include "device-control-dynamic-udev.h"

#include <stdlib.h>
#include <systemd/sd-event.h>
#include <libudev.h>

#include <stdio.h>
#include <string.h>
#include <sys/sysmacros.h>

#include "container.h"
#include "lxc-util.h"
#include "block-util.h"
#include "uevent_injection.h"

#undef _PRINTF_DEBUG_

/**
 * @struct	s_dynamic_device_udev
 * @brief	The data structure for device monitor using libudev.
 */
struct s_dynamic_device_udev {
	struct udev* pudev;					/**< The udev object created by libudev. */
	struct udev_monitor *pudev_monitor;	/**< The udev_monitor object created by libudev.  */
	sd_event_source *libudev_source ;	/**< The sd event source controlled by libudev. */
	containers_t *cs;	/**< TODO */
};

struct s_uevent_device_info {
	const char *devpath;
	const char *subsystem;
	const char *action;
	const char *devtype;
};
typedef struct s_uevent_device_info uevent_device_info_t;

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

static int device_control_dynamic_udev_devevent(dynamic_device_manager_t *ddm);
static int device_control_dynamic_udev_create_info(uevent_device_info_t *udi, lxcutil_dynamic_device_request_t *lddr, struct udev_list_entry *le);
static container_config_t *device_control_dynamic_udev_get_target_container(containers_t *cs, uevent_device_info_t *udi, dynamic_device_entry_items_behavior_t **behavior);
static int device_control_dynamic_udev_rule_judgment(container_config_t *cc, uevent_device_info_t *udi, dynamic_device_entry_items_behavior_t **behavior);
static int device_control_dynamic_udev_create_injection_message(uevent_injection_message_t *uim, uevent_device_info_t *udi, struct udev_list_entry *le);

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
		ret = device_control_dynamic_udev_devevent(ddm);
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
static int device_control_dynamic_udev_devevent(dynamic_device_manager_t *ddm)
{
	int ret = -1;
	struct s_dynamic_device_udev *ddu = NULL;
	struct udev_device *pdev = NULL;
	struct udev_list_entry *le = NULL;
	uevent_device_info_t udi;
	lxcutil_dynamic_device_request_t lddr;
	container_config_t *cc = NULL;
	dynamic_device_entry_items_behavior_t *behavior = NULL;

	ddu = (struct s_dynamic_device_udev*)ddm->ddu;
	pdev = udev_monitor_receive_device(ddu->pudev_monitor);
	if (pdev == NULL)
		goto error_ret;

	memset(&udi, 0, sizeof(udi));
	memset(&lddr, 0, sizeof(lddr));

	le = udev_device_get_properties_list_entry(pdev);
	if (le == NULL)
		goto bypass_ret;	// No data.

	ret = device_control_dynamic_udev_create_info(&udi, &lddr, le);
	if (ret == 0) {
		fprintf(stderr,"udi: action=%s devpath=%s devtype=%s subsystem=%s\n", udi.action, udi.devpath, udi.devtype, udi.subsystem);

		cc = device_control_dynamic_udev_get_target_container(ddu->cs, &udi, &behavior);
		if (cc == NULL)
			goto bypass_ret;	// Not match rule
	} else {
		goto bypass_ret;	// Not match rule
	}

	if (behavior->devnode == 1) {
		lddr.is_create_node = 1;
	}
	lddr.permission = behavior->permission;

	ret = lxcutil_dynamic_device_operation(cc, &lddr);
	if (ret < 0){
		goto error_ret;
	}

	le = udev_device_get_properties_list_entry(pdev);
	if (behavior->injection == 1) {
		uevent_injection_message_t uim;
		pid_t target_pid = 0;

		memset(uim.message, 0 , sizeof(uim.message));
		uim.used = 0;

		fprintf(stderr,"INJECTION: ");

		// TODO uevent injection
		ret = device_control_dynamic_udev_create_injection_message(&uim, &udi, le);
		fprintf(stderr,"\n");
		if (ret < 0){
			goto error_ret;
		}

		target_pid = cc->runtime_stat.lxc->init_pid(cc->runtime_stat.lxc);
		ret = uevent_injection_to_pid(target_pid, &uim);
	}


#if 0
	le = udev_device_get_properties_list_entry(pdev);
	while (le != NULL) {
		const char* elem_name = NULL;
		const char* elem_value = NULL;

		elem_name = udev_list_entry_get_name(le);
		elem_value = udev_list_entry_get_value(le);

		fprintf(stderr,"%s=%s ", elem_name, elem_value);

		le = udev_list_entry_get_next(le);
	}
#endif


bypass_ret:
	udev_device_unref(pdev);

	return 0;

error_ret:
	if (pdev != NULL)
		udev_device_unref(pdev);

	return -1;
}

/**
 * Get point to /dev/ trimmed devname.
 *
 * @param [in]	devnode	String to devname with "/dev/" prefix.
 * @return int
 * @retval	!=NULL	Pointer to trimmed devname.
 * @retval	==NULL	Is not devname.
 */
static const char *trimmed_devname(const char* devnode)
{
	const char *cmpstr = "/dev/";
	const char *pstr = NULL;
	int cmplen = 0;

	cmplen = strlen(cmpstr);

	if (strncmp(devnode, cmpstr, cmplen) == 0) {
		pstr = devnode;
		pstr += cmplen;
	}

	return pstr;
}
static int device_control_dynamic_udev_create_injection_message(uevent_injection_message_t *uim, uevent_device_info_t *udi, struct udev_list_entry *le)
{
	int ret = -1;
	int usage = 0, remain = 0;
	char *buf = NULL;

	remain = sizeof(uim->message) - 1;
	buf = &uim->message[0];

	// add@/devices/pci0000:00/0000:00:08.1/0000:05:00.3/usb4/4-2/4-2:1.0/host3/target3:0:0/3:0:0:0/block/sdb/sdb1
	ret = snprintf(&buf[usage], remain, "%s@%s", udi->action, udi->devpath);
	if ((!(ret < remain)) || ret < 0)
		return -2;

	usage = usage + ret + 1 /*NULL term*/;
	remain = sizeof(uim->message) - 1 - usage;
	if (remain < 0)
		return -2;

	// create body.
	while (le != NULL) {
		const char* elem_name = NULL;
		const char* elem_value = NULL;

		elem_name = udev_list_entry_get_name(le);
		elem_value = udev_list_entry_get_value(le);

		if (strcmp(elem_name, "SEQNUM") == 0) {
			// Skip data
		} else {
			if (strcmp(elem_name, "DEVNAME") == 0) {
				elem_value = trimmed_devname(elem_value);
			}

			ret = snprintf(&buf[usage], remain, "%s=%s", elem_name, elem_value);
			if ((!(ret < remain)) || ret < 0)
				return -2;

			usage = usage + ret + 1 /*NULL term*/;
			remain = sizeof(uim->message) - 1 - usage;
			if (remain < 0)
				return -2;

			fprintf(stderr,"%s=%s ", elem_name, elem_value);
		}

		le = udev_list_entry_get_next(le);
	}

	uim->used = usage;

	return 0;
}

static int device_control_dynamic_udev_create_info(uevent_device_info_t *udi, lxcutil_dynamic_device_request_t *lddr, struct udev_list_entry *le)
{
	lddr->dev_major = -1;
	lddr->dev_minor = -1;

	while (le != NULL) {
		const char* elem_name = NULL;
		const char* elem_value = NULL;

		elem_name = udev_list_entry_get_name(le);
		elem_value = udev_list_entry_get_value(le);

		if (strcmp(elem_name, "ACTION") == 0) {
			udi->action = elem_value;

			if (strcmp(elem_value, "add") == 0) {
				lddr->operation = 1;
			} else if (strcmp(elem_value, "remove") == 0) {
				lddr->operation = 2;
			}
		} else if (strcmp(elem_name, "DEVPATH") == 0) {
			udi->devpath = elem_value;

		} else if (strcmp(elem_name, "SUBSYSTEM") == 0) {
			udi->subsystem = elem_value;

			if (strcmp(elem_value, "block") == 0) {
				lddr->devtype = DEVNODE_TYPE_BLK;
			} else {
				lddr->devtype = DEVNODE_TYPE_CHR;
			}
		} else if (strcmp(elem_name, "DEVTYPE") == 0) {
			udi->devtype = elem_value;

		} else if (strcmp(elem_name, "DEVNAME") == 0) {
			lddr->devnode = elem_value;

		} else if (strcmp(elem_name, "MAJOR") == 0) {
			char *endptr = NULL;
			int value = 0;

			value = strtol(elem_value, &endptr, 10);
			if (elem_value == endptr) {
				lddr->dev_major = -1;
			} else {
				lddr->dev_major = value;
			}
		} else if (strcmp(elem_name, "MINOR") == 0) {
			char *endptr = NULL;
			int value = 0;

			value = strtol(elem_value, &endptr, 10);
			if (elem_value == endptr) {
				lddr->dev_minor = -1;
			} else {
				lddr->dev_minor = value;
			}
		}

		le = udev_list_entry_get_next(le);
	}

	return 0;
}

static container_config_t *device_control_dynamic_udev_get_target_container(containers_t *cs, uevent_device_info_t *udi, dynamic_device_entry_items_behavior_t **behavior)
{
	int num = 0, ret = -1;
	container_config_t *cc = NULL;

	num = cs->num_of_container;

	for(int i=0;i < num;i++) {
		cc = cs->containers[i];
		ret = device_control_dynamic_udev_rule_judgment(cc, udi, behavior);
		if (ret == 1)
			return cc;
	}

	return NULL;
}

static int device_control_dynamic_udev_test_action(const char *actionstr, uevent_action_t *action)
{
	int ret = 0;

	if (actionstr == NULL || action == NULL)
		return ret;

	if (strcmp("add", actionstr) == 0) {
		if (action->add == 1)
			ret = 1;
	} else if (strcmp("remove", actionstr) == 0) {
		if (action->remove == 1)
			ret = 1;
	} else if (strcmp("change", actionstr) == 0) {
		if (action->change == 1)
			ret = 1;
	} else if (strcmp("move", actionstr) == 0) {
		if (action->move == 1)
			ret = 1;
	} else if (strcmp("online", actionstr) == 0) {
		if (action->online == 1)
			ret = 1;
	} else if (strcmp("offline", actionstr) == 0) {
		if (action->offline == 1)
			ret = 1;
	} else if (strcmp("bind", actionstr) == 0) {
		if (action->bind == 1)
			ret = 1;
	} else if (strcmp("unbind", actionstr) == 0) {
		if (action->unbind == 1)
			ret = 1;
	}

	return ret;
}

static int device_control_dynamic_udev_rule_judgment(container_config_t *cc, uevent_device_info_t *udi, dynamic_device_entry_items_behavior_t **behavior)
{
	container_dynamic_device_t *cdd = NULL;
	container_dynamic_device_entry_t *cdde = NULL;
	int ret = 0;
	int result = 0;

	if (cc->runtime_stat.status != CONTAINER_STARTED) {
		// Not running this container.
		return 0;
	}

	cdd = &cc->deviceconfig.dynamic_device;

	dl_list_for_each(cdde, &cdd->dynamic_devlistv2, container_dynamic_device_entry_t, list) {
		if (cdde->devpath == NULL)
			continue;	// No data.

		result = 0;

		if (strncmp(cdde->devpath, udi->devpath, strlen(cdde->devpath)) == 0) {
			// Match devpath
			dynamic_device_entry_items_t *ddei = NULL;

			dl_list_for_each(ddei, &cdde->items, dynamic_device_entry_items_t, list) {
				if (ddei->subsystem == NULL)
					continue;	// No data.

				if (strncmp(ddei->subsystem, udi->subsystem, strlen(ddei->subsystem)) == 0) {
					// Match subsystem

					ret = device_control_dynamic_udev_test_action(udi->action, &ddei->rule.action);
					if (ret != 1)
						continue;	//Not match

					// empty or not
					if (!dl_list_empty(&ddei->rule.devtype_list)) {
						short_string_list_item_t *ssli = NULL;
						dl_list_for_each(ssli, &ddei->rule.devtype_list, short_string_list_item_t, list) {
							if (strncmp(ssli->string, udi->devtype, strlen(udi->devtype)) == 0) {
								result = 1;
							}
						}
					} else {
						result = 1;
					}
				}

				if (result == 1) {
					(*behavior) = &ddei->behavior;
					goto function_return;
				}
			}
		}
	}

function_return:
	return result;
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
int device_control_dynamic_udev_setup(dynamic_device_manager_t *ddm, containers_t *cs, sd_event *event)
{
	struct s_dynamic_device_udev *ddu = NULL;
	struct udev* pudev = NULL;
	struct udev_monitor *pudev_monitor = NULL;
	sd_event_source *libudev_source = NULL;
	int fd = -1;
	int ret = -1;

	if (cs == NULL || cs->ddm == NULL || event == NULL)
		return -2;

	ddu = malloc(sizeof(struct s_dynamic_device_udev));
	if (ddu == NULL)
		goto err_return;

	memset(ddu, 0, sizeof(struct s_dynamic_device_udev));

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

	ddu->pudev = pudev;
	ddu->pudev_monitor = pudev_monitor;
	ddu->libudev_source = libudev_source;
	ddu->cs = cs;

	ddm->ddu = (dynamic_device_udev_t*)ddu;

	return 0;

err_return:
	if (pudev_monitor != NULL)
		udev_monitor_unref(pudev_monitor);

	if (pudev != NULL)
		udev_unref(pudev);

	if (ddu != NULL)
		free(ddu);

	ddm->ddu = NULL;

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
int device_control_dynamic_udev_cleanup(dynamic_device_manager_t *ddm)
{
#if 0
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
#endif
	return 0;

}