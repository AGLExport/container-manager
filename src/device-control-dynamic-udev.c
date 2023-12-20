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
	containers_t *cs;					/**< Pointer to the top data structure for container manager. */
};

/**
 * The function pointer type for subsystem specific assignment rule check.
 *
 * @param [in]	extra_list	Extra rule list from container config.
 * @param [in]	pdev		Pointer to struct udev_device.
 * @param [in]	action		Uevent action.
 * @return int
 * @retval	1	Match to rule.
 * @retval	0	Not match to rule.
 * @retval	-1	Generic error.
 */
typedef int (*extra_checker_func_t)(struct dl_list *extra_list,  struct udev_device *pdev, int action);

/**
 * @struct	s_uevent_device_info
 * @brief	The data structure for device assignment rule check.
 */
struct s_uevent_device_info {
	const char *devpath;	/**< The devpath property from libudev. */
	const char *subsystem;	/**< The subsystem property from libudev. */
	const char *action;		/**< The action property from libudev. */
	const char *devtype;	/**< The devtype property from libudev. */
	extra_checker_func_t checker_func;	/**< The function pointer for subsystem specific assignment rule check. */
};
typedef struct s_uevent_device_info uevent_device_info_t;	/**< typedef for struct s_uevent_device_info. */

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
 * @var		force_exclude_fs
 * @brief	Defined string table to use in filesystem based device masking.
 */
static const char *force_exclude_fs[] = {
	"ext4",
	NULL,
};

static int device_control_dynamic_udev_devevent(dynamic_device_manager_t *ddm);
static int device_control_dynamic_udev_create_info(uevent_device_info_t *udi, lxcutil_dynamic_device_request_t *lddr, struct udev_list_entry *le);
static container_config_t *device_control_dynamic_udev_get_target_container(containers_t *cs, uevent_device_info_t *udi, struct udev_device *pdev
																			, dynamic_device_entry_items_behavior_t **behavior);
static int device_control_dynamic_udev_rule_judgment(container_config_t *cc, uevent_device_info_t *udi, struct udev_device *pdev
													, dynamic_device_entry_items_behavior_t **behavior);
static int device_control_dynamic_udev_create_injection_message(uevent_injection_message_t *uim, uevent_device_info_t *udi, struct udev_list_entry *le);
static int device_control_dynamic_udev_get_uevent_action_code(const char *actionstr);

static int extra_checker_block_device(struct dl_list *extra_list,  struct udev_device *pdev, int action);

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
		(void)device_control_dynamic_udev_devevent(ddm);
	} else {
		;	//nop
	}

	return ret;
}

/**
 * Sub function for uevent monitor.
 * This function analyze uevent and injection to guest if necessary.
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
	if (pdev == NULL) {
		goto error_ret;
	}

	(void) memset(&udi, 0, sizeof(udi));
	(void) memset(&lddr, 0, sizeof(lddr));

	le = udev_device_get_properties_list_entry(pdev);
	if (le == NULL) {
		goto bypass_ret;	// No data.
	}

	ret = device_control_dynamic_udev_create_info(&udi, &lddr, le);
	if (ret == 0) {
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"udi: action=%s devpath=%s devtype=%s subsystem=%s\n", udi.action, udi.devpath, udi.devtype, udi.subsystem);
		#endif

		cc = device_control_dynamic_udev_get_target_container(ddu->cs, &udi, pdev, &behavior);
		if (cc == NULL) {
			goto bypass_ret;	// Not match rule
		}
	} else {
		goto bypass_ret;	// Not match rule
	}

	if ((behavior->devnode == 1) || (behavior->allow == 1)) {
		if (behavior->devnode == 1) {
			lddr.is_create_node = 1;
		}

		if (behavior->allow == 1) {
			lddr.is_allow_device = 1;
		}

		lddr.permission = behavior->permission;

		ret = lxcutil_dynamic_device_operation(cc, &lddr);
		if (ret < 0){
			goto error_ret;
		}
	}

	le = udev_device_get_properties_list_entry(pdev);
	if (behavior->injection == 1) {
		uevent_injection_message_t uim;
		pid_t target_pid = 0;

		(void) memset(uim.message, 0 , sizeof(uim.message));
		uim.used = 0;

		ret = device_control_dynamic_udev_create_injection_message(&uim, &udi, le);
		if (ret < 0){
			goto error_ret;
		}

		target_pid = lxcutil_get_init_pid(cc);
		if (target_pid >= 0) {
			ret = uevent_injection_to_pid(target_pid, &uim);
			if (ret < 0) {
				goto error_ret;
			}
		}
	}

bypass_ret:
	(void) udev_device_unref(pdev);

	return 0;

error_ret:
	if (pdev != NULL) {
		(void) udev_device_unref(pdev);
	}

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
	const char cmpstr[] = "/dev/";
	const char *pstr = NULL;
	size_t cmplen = 0;

	cmplen = strlen(cmpstr);

	if (strncmp(devnode, cmpstr, cmplen) == 0) {
		pstr = &devnode[cmplen];
	}

	return pstr;
}
/**
 * Sub function for uevent monitor.
 * This function create uevent from udev properties list.
 *
 * @param [in,out]	uim	Pointer to uevent_injection_message_t.
 * @param [in]	udi	Pointer to uevent_device_info_t.
 * @param [in]	le	Pointer to udev_list_entry.
 * @return int
 * @retval	0	Success to get device info.
 * @retval	-1	Internal error.
 * @retval	-2	Argument error. (Reserve)
 */
static int device_control_dynamic_udev_create_injection_message(uevent_injection_message_t *uim, uevent_device_info_t *udi, struct udev_list_entry *le)
{
	int ret = -1;
	ssize_t usage = 0, remain = 0;
	char *buf = NULL;

	remain = (ssize_t)sizeof(uim->message) - 1;
	buf = &uim->message[0];

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout,"INJECTION: ");
	#endif

	// add@/devices/pci0000:00/0000:00:08.1/0000:05:00.3/usb4/4-2/4-2:1.0/host3/target3:0:0/3:0:0:0/block/sdb/sdb1
	ret = snprintf(&buf[usage], remain, "%s@%s", udi->action, udi->devpath);
	if (((ssize_t)ret >= remain) || (ret < 0)) {
		return -1;
	}

	usage = usage + (ssize_t)ret + 1 /*NULL term*/;
	remain = ((ssize_t)sizeof(uim->message)) - 1 - usage;
	if (remain < 0) {
		return -1;
	}

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
				if (elem_value == NULL) {
					// It is not device name. This udev entry must drop
					return -1;
				}
			}

			ret = snprintf(&buf[usage], remain, "%s=%s", elem_name, elem_value);
			if (((ssize_t)ret >= remain) || (ret < 0)) {
				return -1;
			}

			usage = usage + ret + 1 /*NULL term*/;
			remain = ((ssize_t)sizeof(uim->message)) - 1 - usage;
			if (remain < 0) {
				return -1;
			}

			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"%s=%s ", elem_name, elem_value);
			#endif
		}

		le = udev_list_entry_get_next(le);
	}

	uim->used = usage;

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout,"\n");
	#endif

	return 0;
}
/**
 * Sub function for uevent monitor.
 * This function create uevent info to use device assignment check and operations from udev properties list.
 *
 * @param [out]	udi		Pointer to uevent_device_info_t.
 * @param [out]	lddr	Pointer to lxcutil_dynamic_device_request_t.
 * @param [in]	le		Pointer to udev_list_entry.
 * @return int
 * @retval	0	Success to get device info.
 * @retval	-1	Internal error.
 * @retval	-2	Argument error. (Reserve)
 */
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
			// set to uevent device info
			udi->action = elem_value;

			// set to lxcutil dynamic device request
			lddr->operation = device_control_dynamic_udev_get_uevent_action_code(elem_value);
		} else if (strcmp(elem_name, "DEVPATH") == 0) {
			// set to uevent device info
			udi->devpath = elem_value;

		} else if (strcmp(elem_name, "SUBSYSTEM") == 0) {
			// set to uevent device info
			udi->subsystem = elem_value;

			// set to lxcutil dynamic device request
			if (strcmp(elem_value, dev_subsys_block) == 0) {
				lddr->devtype = DEVNODE_TYPE_BLK;
				udi->checker_func = extra_checker_block_device;
			} else if (strcmp(elem_value, dev_subsys_net) == 0) {
				lddr->devtype = DEVNODE_TYPE_NET;
			} else {
				lddr->devtype = DEVNODE_TYPE_CHR;
			}
		} else if (strcmp(elem_name, "DEVTYPE") == 0) {
			// set to uevent device info
			udi->devtype = elem_value;

		} else if (strcmp(elem_name, "DEVNAME") == 0) {
			// set to lxcutil dynamic device request
			lddr->devnode = elem_value;

		} else if (strcmp(elem_name, "MAJOR") == 0) {
			char *endptr = NULL;
			int value = 0;

			// set to lxcutil dynamic device request
			value = strtol(elem_value, &endptr, 10);
			if (elem_value == endptr) {
				lddr->dev_major = -1;
			} else {
				lddr->dev_major = value;
			}
		} else if (strcmp(elem_name, "MINOR") == 0) {
			char *endptr = NULL;
			int value = 0;

			// set to lxcutil dynamic device request
			value = strtol(elem_value, &endptr, 10);
			if (elem_value == endptr) {
				lddr->dev_minor = -1;
			} else {
				lddr->dev_minor = value;
			}
		} else {
			;	//skip this data
		}

		le = udev_list_entry_get_next(le);
	}

	return 0;
}
/**
 * Sub function for uevent monitor.
 * This function check device assignment to all containers. It return behavior for target device.
 *
 * @param [in]	cs		Pointer to containers_t.
 * @param [in]	udi		Pointer to uevent_device_info_t.
 * @param [in]	pdev	Pointer to struct udev_device.
 * @param [out]	le		Double pointer to dynamic_device_entry_items_behavior_t.
 * @return int
 * @retval	!= NULL	A container_config_t for device assignment target.
 * @retval	NULL	Not found target.
 */
static container_config_t *device_control_dynamic_udev_get_target_container(containers_t *cs, uevent_device_info_t *udi, struct udev_device *pdev
																			, dynamic_device_entry_items_behavior_t **behavior)
{
	int num = 0, ret = -1;
	container_config_t *cc = NULL;

	num = cs->num_of_container;

	for(int i=0;i < num;i++) {
		cc = cs->containers[i];
		ret = device_control_dynamic_udev_rule_judgment(cc, udi, pdev, behavior);
		if (ret == 1) {
			return cc;
		}
	}

	return NULL;
}
/**
 * Sub function for uevent monitor.
 * Test uevent to match handling rule.
 *
 * @param [in]	actionstr	The string of uevent action.
 * @param [in]	action		Pointer to uevent_action_t.
 * @return int
 * @retval	0<	Target uevent code to match handling rule.
 * @retval	0	An action of actionstr does not match handling rule.
 * @retval	<0	Internal error (Not use).
 */
static int device_control_dynamic_udev_get_uevent_action_code(const char *actionstr)
{
	int ret = DCD_UEVENT_ACTION_NON;

	if (actionstr == NULL) {
		return ret;
	}

	if (strcmp("add", actionstr) == 0) {
		ret = DCD_UEVENT_ACTION_ADD;
	} else if (strcmp("remove", actionstr) == 0) {
		ret = DCD_UEVENT_ACTION_REMOVE;
	} else if (strcmp("change", actionstr) == 0) {
		ret = DCD_UEVENT_ACTION_CHANGE;
	} else if (strcmp("move", actionstr) == 0) {
		ret = DCD_UEVENT_ACTION_MOVE;
	} else if (strcmp("online", actionstr) == 0) {
		ret = DCD_UEVENT_ACTION_ONLINE;
	} else if (strcmp("offline", actionstr) == 0) {
		ret = DCD_UEVENT_ACTION_OFFLINE;
	} else if (strcmp("bind", actionstr) == 0) {
		ret = DCD_UEVENT_ACTION_BIND;
	} else if (strcmp("unbind", actionstr) == 0) {
		ret = DCD_UEVENT_ACTION_UNBIND;
	} else {
		ret = DCD_UEVENT_ACTION_NON;
	}

	return ret;
}
/**
 * Sub function for uevent monitor.
 * Test uevent to match handling rule.
 *
 * @param [in]	actionstr	The string of uevent action.
 * @param [in]	action		Pointer to uevent_action_t.
 * @return int
 * @retval	0<	Target uevent code to match handling rule.
 * @retval	0	An action of actionstr does not match handling rule.
 * @retval	<0	Internal error (Not use).
 */
static int device_control_dynamic_udev_test_action(const char *actionstr, uevent_action_t *action)
{
	int ret = DCD_UEVENT_ACTION_NON;
	int action_code = DCD_UEVENT_ACTION_NON;

	if ((actionstr == NULL) || (action == NULL)) {
		return ret;
	}

	action_code = device_control_dynamic_udev_get_uevent_action_code(actionstr);

	if (action_code == DCD_UEVENT_ACTION_ADD) {
		if (action->add == 1) {
			ret = DCD_UEVENT_ACTION_ADD;
		}
	} else if (action_code == DCD_UEVENT_ACTION_REMOVE) {
		if (action->remove == 1) {
			ret = DCD_UEVENT_ACTION_REMOVE;
		}
	} else if (action_code == DCD_UEVENT_ACTION_CHANGE) {
		if (action->change == 1) {
			ret = DCD_UEVENT_ACTION_CHANGE;
		}
	} else if (action_code == DCD_UEVENT_ACTION_MOVE) {
		if (action->move == 1) {
			ret = DCD_UEVENT_ACTION_MOVE;
		}
	} else if (action_code == DCD_UEVENT_ACTION_ONLINE) {
		if (action->online == 1) {
			ret = DCD_UEVENT_ACTION_ONLINE;
		}
	} else if (action_code == DCD_UEVENT_ACTION_OFFLINE) {
		if (action->offline == 1) {
			ret = DCD_UEVENT_ACTION_OFFLINE;
		}
	} else if (action_code == DCD_UEVENT_ACTION_BIND) {
		if (action->bind == 1) {
			ret = DCD_UEVENT_ACTION_BIND;
		}
	} else if (action_code == DCD_UEVENT_ACTION_UNBIND) {
		if (action->unbind == 1) {
			ret = DCD_UEVENT_ACTION_UNBIND;
		}
	} else {
		ret = DCD_UEVENT_ACTION_NON;
	}

	return ret;
}
/**
 * Sub function for uevent monitor.
 * This function test to match between the dynamic device handling rule and uevent device info.
 * When that is matched, it provide reference to the behavior data inside a container config.
 *
 * @param [in]	cc			Pointer to container_config_t.
 * @param [in]	udi			Pointer to uevent_device_info_t.
 * @param [in]	pdev		Pointer to struct udev_device.
 * @param [out]	behavior	Double pointer to dynamic_device_entry_items_behavior_t.  It use to get reference to the behavior data.
 * @return int
 * @retval	0	Success to event handling.
 * @retval	-1	Internal error (Not use).
 */
static int device_control_dynamic_udev_rule_judgment(container_config_t *cc, uevent_device_info_t *udi, struct udev_device *pdev
													, dynamic_device_entry_items_behavior_t **behavior)
{
	container_dynamic_device_t *cdd = NULL;
	container_dynamic_device_entry_t *cdde = NULL;
	int ret = 0;
	int action_code = 0;
	int result = 0;

	if (cc->runtime_stat.status != CONTAINER_STARTED) {
		// Not running this container.
		return 0;
	}

	cdd = &cc->deviceconfig.dynamic_device;

	dl_list_for_each(cdde, &cdd->dynamic_devlist, container_dynamic_device_entry_t, list) {
		if (cdde->devpath == NULL) {
			continue;	// No data.
		}

		result = 0;

		if (strncmp(cdde->devpath, udi->devpath, strlen(cdde->devpath)) == 0) {
			// Match devpath :
			// cdde = "/a/b/c"  udi = "/a/b/c/d" this case shall judge "match".
			// cdde = "/a/b/c"  udi = "/a/b/" this case shall judge "not match".
			dynamic_device_entry_items_t *ddei = NULL;

			dl_list_for_each(ddei, &cdde->items, dynamic_device_entry_items_t, list) {
				if (ddei->subsystem == NULL) {
					continue;	// No data.
				}

				if (strcmp(ddei->subsystem, udi->subsystem) == 0) {
					// Match subsystem
					// ddei = "hid"     udi = "hid" this case shall judge "match".
					// ddei = "hid"     udi = "hidraw" this case shall judge "not match".
					// ddei = "hidraw"  udi = "hid" this case shall judge "not match".

					action_code = device_control_dynamic_udev_test_action(udi->action, &ddei->rule.action);
					if (action_code == 0) {
						continue;	//Not match
					}

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

				if ((udi->checker_func != NULL) && (result == 1)) {
					// Have a extra rule?
					if (dl_list_empty(&ddei->rule.extra_list) == 0) {
						ret = udi->checker_func(&ddei->rule.extra_list, pdev, action_code);
						if (ret != 1) {
							result = 0;
						}
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
 * Extra uevent checker function for block device.
 *
 * @param [in]	extra_list	Pointer to extra rule lest inside a container config.
 * @param [in]	pdev		Pointer to struct udev_device.
 * @param [in]	action		Uevent action.
 * @retval	1	Match to rule.
 * @retval	0	Not match to rule.
 * @retval	-1	Internal error (Not use).
 */
static int extra_checker_block_device(struct dl_list *extra_list,  struct udev_device *pdev, int action)
{
	int ret = -1, result = 0;
	struct udev_list_entry *le = NULL;
	dynamic_device_entry_items_rule_extra_t *pre= NULL;
	const char *devnode = NULL;

	// Block device is only to check in add event case.
	if (action != DCD_UEVENT_ACTION_ADD) {
		return 1;
	}

	// Analyze udev properties list.
	le = udev_device_get_properties_list_entry(pdev);
	while (le != NULL) {
		const char* elem_name = NULL;
		const char* elem_value = NULL;

		elem_name = udev_list_entry_get_name(le);
		elem_value = udev_list_entry_get_value(le);

		if (strcmp(elem_name, "DEVNAME") == 0) {
			devnode = elem_value;
		}

		le = udev_list_entry_get_next(le);
	}

	if (devnode != NULL) {
		block_device_info_t bdi;

		(void) memset(&bdi, 0 , sizeof(bdi));

		ret = block_util_getfs(devnode, &bdi);
		if (ret == 0) {
			for(int i=0; force_exclude_fs[i] != NULL; i++) {
				if (strcmp(bdi.type, force_exclude_fs[i]) == 0) {
					result = 0;
					goto bypass_ret;
				}
			}

			dl_list_for_each(pre, extra_list, dynamic_device_entry_items_rule_extra_t, list) {
				if (pre->checker == NULL || pre->value == NULL) {
					continue;
				}

				if (strcmp(pre->checker, "exclude-fs") == 0) {
					result = 1;
					if (strcmp(bdi.type, pre->value) == 0) {
						result = 0;
					}
					break;
				} else if (strcmp(pre->checker, "include-fs") == 0) {
					result = 0;
					if (strcmp(bdi.type, pre->value) == 0) {
						result = 1;
					}
					break;
				} else {
					;	//nop
				}
			}
		}
	}

bypass_ret:

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

	if ((cs == NULL) || (cs->ddm == NULL) || (event == NULL)) {
		return -2;
	}

	ddu = malloc(sizeof(struct s_dynamic_device_udev));
	if (ddu == NULL) {
		goto err_return;
	}

	(void) memset(ddu, 0, sizeof(struct s_dynamic_device_udev));

	pudev = udev_new();
	if (pudev == NULL) {
		goto err_return;
	}

	pudev_monitor = udev_monitor_new_from_netlink(pudev,"kernel");
	if (pudev_monitor == NULL) {
		goto err_return;
	}

	ret = udev_monitor_enable_receiving(pudev_monitor);
	if (ret < 0) {
		goto err_return;
	}

	fd = udev_monitor_get_fd(pudev_monitor);
	if (fd < 0) {
		goto err_return;
	}

	ret = sd_event_add_io(event, &libudev_source, fd, EPOLLIN, udev_event_handler, ddm);
	if (ret < 0) {
		goto err_return;
	}

	ddu->pudev = pudev;
	ddu->pudev_monitor = pudev_monitor;
	ddu->libudev_source = libudev_source;
	ddu->cs = cs;

	ddm->ddu = (dynamic_device_udev_t*)ddu;

	return 0;

err_return:
	if (pudev_monitor != NULL) {
		(void) udev_monitor_unref(pudev_monitor);
	}

	if (pudev != NULL) {
		(void) udev_unref(pudev);
	}

	if (ddu != NULL) {
		(void) free(ddu);
	}

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
	struct s_dynamic_device_udev *ddu = NULL;

	ddu = (struct s_dynamic_device_udev*)ddm->ddu;

	if (ddu->libudev_source != NULL) {
		(void) sd_event_source_disable_unref(ddu->libudev_source);
	}

	if (ddu->pudev_monitor != NULL) {
		(void) udev_monitor_unref(ddu->pudev_monitor);
	}

	if (ddu->pudev != NULL) {
		(void) udev_unref(ddu->pudev);
	}

	(void) free(ddu);

	return 0;
}