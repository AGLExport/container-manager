/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	device-control-static.c
 * @brief	device control sub block for static device management.
 */
#undef _PRINTF_DEBUG_

#include "device-control.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>

#include "cm-utils.h"

static int devc_static_devnode_scan(container_static_device_t *sdevc);
static int devc_gpionode_scan(container_static_device_t *devc);
static int devc_iionode_scan(container_static_device_t *sdevc);
static int devc_netbridge_setup(container_manager_config_t *cmc);

/**
 * Start up time device initialization.
 * This function scan and setup device node, sysfs gpio and sysfs iio with device node.
 *
 * @param [in]	cs	Pointer to containers_t.
 * @return int
 * @retval  0 Success.
 * @retval -1 Device scan and/or setup critical error.
 */
int devc_early_device_setup(containers_t *cs)
{
	int num;
	int ret = 1;
	int result = -1;
	container_config_t *cc = NULL;

	(void)devc_netbridge_setup(cs->cmcfg);

	num = cs->num_of_container;

	for(int i=0;i < num;i++) {
		cc = cs->containers[i];

		// run static device scan
		ret = devc_static_devnode_scan(&cc->deviceconfig.static_device);
		if (ret < 0) {
			goto err_ret;
		}

		// run gpio export
		ret = devc_gpionode_scan(&cc->deviceconfig.static_device);
		if (ret < 0) {
			goto err_ret;
		}

		ret = devc_iionode_scan(&cc->deviceconfig.static_device);
		if (ret < 0) {
			goto err_ret;
		}
	}

	return 0;

err_ret:

	return result;
}
/**
 * Start up time device initialization device node sub.
 * This function do device node scanning only.
 *
 * @param [in]	sdevc	Pointer to container_static_device_t.
 * @return int
 * @retval  0 Success to scan device node.
 * @retval -1 Device scan error.
 * @retval -2 Syscall error.
 */
static int devc_static_devnode_scan(container_static_device_t *sdevc)
{
	int ret = 1;
	int result = -1;
	container_static_device_elem_t *develem = NULL;

	// static device node
	dl_list_for_each(develem, &sdevc->static_devlist, container_static_device_elem_t, list) {
		struct stat sb = {0};

		if (develem->devnode == NULL) {
			// This type data must not created in data parser.
			result = -1;
			goto err_ret;
		}

		ret = stat(develem->devnode, &sb);
		if (ret < 0) {
			// no device node
			develem->is_valid = 0;
			continue;
		}

		// Check device node type
		if(S_ISCHR(sb.st_mode)) {
			develem->devtype = DEVNODE_TYPE_CHR;
		} else if (S_ISBLK(sb.st_mode)) {
			develem->devtype = DEVNODE_TYPE_BLK;
		} else {
			develem->devtype = DEVICE_TYPE_UNKNOWN;	//may not run this line
		}

		if (develem->devtype != 0) {
			// Set major and minor num
			develem->major = major(sb.st_rdev);
			develem->minor = minor(sb.st_rdev);

			// Set valid flag
			develem->is_valid = 1;
		} else {
			develem->is_valid = 0;
		}

		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"devc: device file %s detect major = %d, minor = %d\n", develem->devnode, develem->major, develem->minor);
		#endif
	}

	return 0;

err_ret:

	return result;
}
/**
 * @var		gpio_direction_table
 * @brief	Table of gpio direction setting to use convert from string to code.
 */
static const char *gpio_direction_table[] = {
	"in",	// default - not set 0 - in
	"in",	// DEVGPIO_DIRECTION_IN
	"out",	// DEVGPIO_DIRECTION_OUT
	"low",	// DEVGPIO_DIRECTION_LOW
	"high"	// DEVGPIO_DIRECTION_HIGH
};
/**
 * @var		gpio_export_node
 * @brief	Static string data for gpio sysfs exportation.
 */
static const char gpio_export_node[] = "/sys/class/gpio/export";
/**
 * Start up time device initialization gpio sub.
 * This function export gpio port and set direction and initial value.
 *
 * @param [in]	sdevc	Pointer to
 * @return int
 * @retval  0 Success.
 * @retval -1 Configuration error.
 */
static int devc_gpionode_scan(container_static_device_t *sdevc)
{
	int ret = 1;
	int result = -1;
	char buf[1024];
	char directionbuf[128];
	ssize_t slen = 0, buflen = 0;
	container_static_gpio_elem_t *gpioelem = NULL;

	// static device node
	dl_list_for_each(gpioelem, &sdevc->static_gpiolist, container_static_gpio_elem_t, list) {

		if (gpioelem->from == NULL) {
			// This type data must not created in data parser.
			result = -1;
			goto err_ret;
		}

		// is exported?
		ret = node_check(gpioelem->from);
		if (ret == -1) {
			// gpio is not exported, need to export
			buf[0] = '\0';
			buflen = (ssize_t)sizeof(buf) - 1;

			slen = (ssize_t)snprintf(buf, buflen, "%d", gpioelem->port);
			if (slen >= buflen) {
				continue; //May not cause this error.
			}

			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"devc: gpio node %s will export\n", buf);
			#endif
			ret = once_write(gpio_export_node, buf, slen);
			if (ret == 0) {
				ret = node_check(gpioelem->from);
				if (ret == -1) {
					// gpio export error. In this case that port can't use gpio,
					// that port is already assign other function may be.
					// Skip this port
					continue;
				}
			}
		}

		// direction setting
		buf[0] = '\0';
		directionbuf[0] = '\0';
		buflen = (ssize_t)sizeof(buf) - 1;

		slen = (ssize_t)snprintf(buf, buflen, "%s/direction", gpioelem->from);
		if (slen >= buflen) {
			continue; //Skip port setup, may not cause this error.
		}

		ret = once_write(buf, gpio_direction_table[gpioelem->portdirection], strlen(gpio_direction_table[gpioelem->portdirection]));
		if (ret != 0) {
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"devc: gpio node %s direction set %s is fail\n", gpioelem->from,  gpio_direction_table[gpioelem->portdirection]);
			#endif
			continue; //Skip port setup, may not cause this error.
		}
		gpioelem->is_valid = 1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"devc: gpio node %s is valid = %d\n", gpioelem->from, gpioelem->is_valid);
		#endif
	}

	return 0;

err_ret:

	return result;
}
/**
 * Start up time device initialization iio sub.
 * This function scan sysfs node and device node.
 *
 * @param [in]	sdevc	Pointer to container_static_device_t.
 * @return int
 * @retval  0 Success.
 * @retval -1 Configuration error.
 */
static int devc_iionode_scan(container_static_device_t *sdevc)
{
	int ret = 1;
	int result = -1;
	container_static_iio_elem_t *iioelem = NULL;

	// static device node
	dl_list_for_each(iioelem, &sdevc->static_iiolist, container_static_iio_elem_t, list) {

		if ((iioelem->sysfrom == NULL) || (iioelem->systo == NULL)) {
			// This type data must not created in data parser.
			result = -1;
			goto err_ret;
		}

		// is available?
		ret = node_check(iioelem->sysfrom);
		if (ret != 0) {
			// sysfs iio node is not available. skip.
			iioelem->is_sys_valid = 0;
			continue;
		}
		iioelem->is_sys_valid = 1;

		// optional info check
		if ((iioelem->devfrom != NULL) && (iioelem->devto != NULL) && (iioelem->devnode != NULL)) {
			// dev node option is enabled.
			struct stat sb = {0};

			ret = stat(iioelem->devnode, &sb);
			if (ret < 0) {
				// no device node, skip.
				iioelem->is_dev_valid = 0;
				continue;
			}

			// Check device node type
			if(!S_ISCHR(sb.st_mode)) {
				// iio dev node must be char device, skip.
				iioelem->is_dev_valid = 0;
				continue;
			}

			// Set major and minor num
			iioelem->major = major(sb.st_rdev);
			iioelem->minor = minor(sb.st_rdev);

			// Set valid flag
			iioelem->is_dev_valid = 1;

		} else {
			iioelem->is_dev_valid = 0;
		}

		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"devc: iio node %s / %s is valid = %d\n", iioelem->sysfrom, iioelem->devfrom, iioelem->is_valid);
		#endif
	}

	return 0;

err_ret:

	return result;
}
/**
 * Start up time network initialization for bridge device.
 * This function create network bridge.
 *
 * @param [in]	cmc	Pointer to container_manager_config_t.
 * @return int
 * @retval  0 Success to operations.
 * @retval -1 Configuration error. (Reserve)
 * @retval -2 Syscall error.
 */
static int devc_netbridge_setup(container_manager_config_t *cmc)
{
	int ret = -1;
	int result = 0;
	int sock = -1;
	char buf[IFNAMSIZ+1];
	container_manager_bridge_config_t *elem = NULL;

	sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (sock < 0) {
		return -2;
	}

	// static device node
	dl_list_for_each(elem, &cmc->bridgelist, container_manager_bridge_config_t, list) {

		if (elem->name == NULL) {
			// This type data must not created in data parser.
			result = -1;
			continue;
		}

		(void) memset(buf, 0, sizeof(buf));
		(void) strncpy(buf, elem->name, IFNAMSIZ);

		ret = ioctl(sock, SIOCBRADDBR, buf);
		if ((ret < 0) && (errno != EEXIST)) {
			result = -2;
		}
	}

	(void) close(sock);

	return result;
}
