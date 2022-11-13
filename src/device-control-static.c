/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	device-control.c
 * @brief	device control block for container manager
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
static int devc_netbridge_setup(container_manager_config_t *cmc);


/**
 * Start up time device initialization
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Device scan critical error.
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
		if (ret < 0)
			goto err_ret;

		// run gpio export
		ret = devc_gpionode_scan(&cc->deviceconfig.static_device);
		if (ret < 0)
			goto err_ret;
	}

	return 0;

err_ret:
	
	return result;
}
/**
 * Start up time device initialization device node sub
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
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
			// This type data must not created in data perser.
			result = -1;
			goto err_ret;
		}

		ret = stat(develem->devnode, &sb);
		if (ret < 0) {
			// no device node
			develem->is_valid = 0;
			continue;
		}

		// Check devide node type
		if(S_ISCHR(sb.st_mode))
			develem->devtype = DEVNODE_TYPE_CHR;
		else if (S_ISBLK(sb.st_mode))
			develem->devtype = DEVNODE_TYPE_BLK;

		if (develem->devtype != 0) {
			// Set major and minor num
			develem->major = major(sb.st_rdev);
			develem->minor = minor(sb.st_rdev);

			// Set valid flag
			develem->is_valid = 1;
		} else
			develem->is_valid = 0;

		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"devc: device file %s detect major = %d, minor = %d\n", develem->devnode, develem->major, develem->minor);
		#endif
	}

	return 0;

err_ret:

	return result;
}

static const char *gpio_direction_table[] = {
	"in",	// default - not set 0 - in
	"in",	// DEVGPIO_DIRECTION_IN
	"out",	// DEVGPIO_DIRECTION_OUT
	"low",	// DEVGPIO_DIRECTION_LOW
	"high"	// DEVGPIO_DIRECTION_HIGH
};
/**
 * Start up time device initialization
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Device scan error.
 * @retval -2 Syscall error.
 * @retval -3 Memory allocation error. 
 */
static const char gpio_export_node[] = "/sys/class/gpio/export";
static int devc_gpionode_scan(container_static_device_t *sdevc)
{
	int ret = 1;
	int result = -1;
	char buf[1024];
	char directionbuf[128];
	int slen = 0, buflen = 0;
	const char *pdevtype = NULL;
	container_static_gpio_elem_t *gpioelem = NULL;

	// static device node
	dl_list_for_each(gpioelem, &sdevc->static_gpiolist, container_static_gpio_elem_t, list) {

		if (gpioelem->from == NULL) {
			// This type data must not created in data perser.
			result = -1;
			goto err_ret;
		}

		// is exported?
		ret = node_check(gpioelem->from);
		if (ret == -1) {
			// gpio is not expoted, need to export
			buf[0] = '\0';
			buflen = sizeof(buf) - 1;

			slen = snprintf(buf, buflen, "%d", gpioelem->port);
			if (!(slen < buflen))
				continue; //May not cause this error.

			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"devc: gpio node %s will export\n", buf);
			#endif
			ret = onece_write(gpio_export_node, buf, slen);
			if (ret == 0) {
				ret = node_check(gpioelem->from);
				if (ret == -1) {
					// gpio export errot. In this case that poart can't use gpio,
					// that port is already assign other function may be.
					// Skip this port 
					continue;
				}
			}
		}

		// direction setting
		buf[0] = '\0';
		directionbuf[0] = '\0';
		buflen = sizeof(buf) - 1;

		slen = snprintf(buf, buflen, "%s/direction", gpioelem->from);
		if (!(slen < buflen))
			continue; //Skip port setup, may not cause this error.

		ret = onece_write(buf, gpio_direction_table[gpioelem->portdirection], strlen(gpio_direction_table[gpioelem->portdirection]));
		if (ret != 0) {
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"devc: gpio node %s direction set %s is fail\n", gpioelem->from,  gpio_direction_table[gpioelem->portdirection]);
			#endif
			continue; //Skip port setup, may not cause this error.
		}
		gpioelem->is_valid = 1;
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"devc: gpio node %s is valid = %d\n", gpioelem->from, gpioelem->is_valid);
		#endif
	}

	return 0;

err_ret:

	return result;
}
/**
 * Start up time device initialization
 *
 * @param [in]	cs	Preconstructed containers_t
 * @return int
 * @retval  0 Success.
 * @retval -1 Device scan error.
 * @retval -2 Syscall error.
 * @retval -3 Memory allocation error. 
 */
static int devc_netbridge_setup(container_manager_config_t *cmc)
{
	int ret = -1;
	int result = 0;
	int sock = -1;
	char buf[IFNAMSIZ+1];
	container_manager_bridge_config_t *elem = NULL;

	sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (sock < 0)
		return -2;

	// static device node
	dl_list_for_each(elem, &cmc->bridgelist, container_manager_bridge_config_t, list) {

		if (elem->name == NULL) {
			// This type data must not created in data perser.
			result = -1;
			continue;
		}

		memset(buf, 0, sizeof(buf));
		strncpy(buf, elem->name, IFNAMSIZ);

		ret = ioctl(sock, SIOCBRADDBR, buf);
		if (ret < 0 && errno != EEXIST) {
			result = -1;
		}
	}

	close(sock);

	return result;
}