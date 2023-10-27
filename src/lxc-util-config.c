/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	lxc-util.c
 * @brief	LXC control interface for container manager use.
 */

#include "lxc-util.h"

#include <stdio.h>
#include <string.h>
#include <lxc/lxccontainer.h>

#ifdef _PRINTF_DEBUG_
#define	PRINTF_DEBUG_CONFIG_OUT	(1)
#endif

/**
 * Create lxc config from container config baseconfig sub part.
 *
 * @param [in]	plxc	The lxc container instance to set config.
 * @param [in]	bc		Pointer to container_baseconfig_t.
 * @return int
 * @retval 0	Success to set lxc config from bc.
 * @retval -1	Got lxc error.
 * @retval -2	A bytes of config string is larger than buffer size. Critical case only.
 */
static int lxcutil_set_config_base(struct lxc_container *plxc, container_baseconfig_t *bc)
{
	int ret = 1;
	int result = -1;
	bool bret = false;
	char buf[1024];
	ssize_t buflen = 0, slen = 0;

	(void) memset(buf,0,sizeof(buf));

	// rootfs
	slen = (ssize_t)snprintf(buf, (sizeof(buf)-1u), "dir:%s", bc->rootfs.path);
	if (slen >= ((ssize_t)sizeof(buf) - 1)) {
		goto err_ret;
	}

	bret = plxc->set_config_item(plxc, "lxc.rootfs.path", buf);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.rootfs.path", buf);
		#endif
		goto err_ret;
	}

	// extradisk - optional
	if (!dl_list_empty(&bc->extradisk_list)) {
		container_baseconfig_extradisk_t *exdisk = NULL;

		dl_list_for_each(exdisk, &bc->extradisk_list, container_baseconfig_extradisk_t, list) {
			(void) memset(buf, 0, sizeof(buf));
			buflen = (ssize_t)sizeof(buf) -1;

			if (exdisk->mode == DISKMOUNT_TYPE_RW) {
				slen = (ssize_t)snprintf(buf, buflen, "%s %s none bind,rw,create=dir", exdisk->from, exdisk->to);
			} else {
				slen = (ssize_t)snprintf(buf, buflen, "%s %s none bind,ro,create=dir", exdisk->from, exdisk->to);
			}

			if (slen < buflen) {
				bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
					#endif
					goto err_ret;
				}
			}
		}
	}

	// halt and reboot signal - mandatory, if this entry didn't have config, default value set in parser.
	bret = plxc->set_config_item(plxc, "lxc.signal.halt", bc->lifecycle.halt);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.signal.halt", bc->lifecycle.halt);
		#endif
		goto err_ret;
	}

	bret = plxc->set_config_item(plxc, "lxc.signal.reboot", bc->lifecycle.reboot);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.signal.reboot", bc->lifecycle.reboot);
		#endif
		goto err_ret;
	}

	// cap - optional
	if (bc->cap.drop != NULL) {
		if (strlen(bc->cap.drop) > 0u){
			bret = plxc->set_config_item(plxc, "lxc.cap.drop", bc->cap.drop);
			if (bret == false) {
				result = -1;
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.cap.drop", bc->cap.drop);
				#endif
				goto err_ret;
			}
		}
	}

	if (bc->cap.keep != NULL) {
		if (strlen(bc->cap.keep) > 0u){
			bret = plxc->set_config_item(plxc, "lxc.cap.keep", bc->cap.keep);
			if (bret == false) {
				result = -1;
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.cap.keep", bc->cap.keep);
				#endif
				goto err_ret;
			}
		}
	}

	// idmap - optional
	if (bc->idmaps.enabled == 1) {
		(void) memset(buf,0,sizeof(buf));
		ret = snprintf(buf,sizeof(buf),"u %d %d %d", bc->idmaps.uid.guest_root_id, bc->idmaps.uid.host_start_id, bc->idmaps.uid.num_of_id);
		if ((ssize_t)ret >= (ssize_t)sizeof(buf)) {
			result = -2;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base idmap (uid) too long parameter.\n");
			#endif
			goto err_ret;
		}

		bret = plxc->set_config_item(plxc, "lxc.idmap", buf);
		if (bret == false) {
			result = -1;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.idmap", buf);
			#endif
			goto err_ret;
		}

		(void) memset(buf,0,sizeof(buf));
		ret = snprintf(buf,sizeof(buf),"g %d %d %d", bc->idmaps.gid.guest_root_id, bc->idmaps.gid.host_start_id, bc->idmaps.gid.num_of_id);
		if ((ssize_t)ret >= (ssize_t)sizeof(buf)) {
			result = -2;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base idmap (gid) too long parameter.\n");
			#endif
			goto err_ret;
		}

		bret = plxc->set_config_item(plxc, "lxc.idmap", buf);
		if (bret == false) {
			result = -1;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.idmap", buf);
			#endif
			goto err_ret;
		}

	}

	// mount.auto settings
	if (bc->extended.shmounts != NULL) {
		// The shmount option was enabled.
		buf[0] = '\0';
		ret = snprintf(buf,sizeof(buf),"cgroup:ro proc:mixed sys:mixed shmounts:%s", bc->extended.shmounts);
	} else {
		// The shmount option was not enabled. Set default options.
		buf[0] = '\0';
		ret = snprintf(buf,sizeof(buf),"cgroup:ro proc:mixed sys:mixed");
	}
	bret = plxc->set_config_item(plxc, "lxc.mount.auto", buf);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.auto", buf);
		#endif
		goto err_ret;
	}

	// tty setting
	//tty
	//value or default value is already set parser part.
	buf[0] = '\0';
	ret = snprintf(buf,sizeof(buf),"%d", bc->tty.tty_max);
	if ((ssize_t)ret >= (ssize_t)sizeof(buf)) {
		result = -2;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base tty max too long parameter.\n");
		#endif
		goto err_ret;
	}
	bret = plxc->set_config_item(plxc, "lxc.tty.max", buf);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s fail.\n", "lxc.tty.max");
		#endif
		goto err_ret;
	}

	//pty
	//value or default value is already set parser part.
	buf[0] = '\0';
	ret = snprintf(buf,sizeof(buf),"%d", bc->tty.pty_max);
	if ((ssize_t)ret >= (ssize_t)sizeof(buf)) {
		result = -2;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base tty max too long parameter.\n");
		#endif
		goto err_ret;
	}
	bret = plxc->set_config_item(plxc, "lxc.pty.max", buf);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s fail.\n", "lxc.tty.max");
		#endif
		goto err_ret;
	}

	return 0;

err_ret:

	return result;
}
/**
 * Create lxc config from container config resourceconfig sub part.
 *
 * @param [in]	plxc	The lxc container instance to set config.
 * @param [in]	rsc		Pointer to container_resourceconfig_t.
 * @return int
 * @retval 0	Success to set lxc config from rsc.
 * @retval -1	Got lxc error.
 * @retval -2	A bytes of config string is larger than buffer size. Critical case only.
 */
static int lxcutil_set_config_resource(struct lxc_container *plxc, container_resourceconfig_t *rsc)
{
	int result = -1;
	bool bret = false;
	char buf[1024];
	ssize_t slen = 0, buflen = 0;
	container_resource_elem_t *melem = NULL;

	(void) memset(buf,0,sizeof(buf));

	dl_list_for_each(melem, &rsc->resource.resourcelist, container_resource_elem_t, list) {
		buflen = (ssize_t)sizeof(buf) - 1;
		buf[0] = '\0';

		if (melem->type == RESOURCE_TYPE_CGROUP) {
			if ((melem->object == NULL) || (melem->value == NULL)) {
				continue;	//drop data
			}

			slen = (ssize_t)snprintf(buf, buflen, "lxc.cgroup.%s", melem->object);
			if (slen >= buflen) {
				continue;	// buffer over -> drop data
			}

			bret = plxc->set_config_item(plxc, buf, melem->value);
			if (bret == false) {
				result = -1;
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"lxcutil: lxcutil_set_config_resource set config %s = %s fail.\n", buf, melem->value);
				#endif
				goto err_ret;
			}
		} else 	if (melem->type == RESOURCE_TYPE_PRLIMIT) {
			if (melem->object == NULL || melem->value == NULL) {
				continue;	//drop data
			}

			slen = (ssize_t)snprintf(buf, buflen, "lxc.prlimit.%s", melem->object);
			if (slen >= buflen) {
				continue;	// buffer over -> drop data
			}

			bret = plxc->set_config_item(plxc, buf, melem->value);
			if (bret == false) {
				result = -1;
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"lxcutil: lxcutil_set_config_resource set config %s = %s fail.\n", buf, melem->value);
				#endif
				goto err_ret;
			}
		} else if (melem->type == RESOURCE_TYPE_SYSCTL) {
			if (melem->object == NULL || melem->value == NULL) {
				continue;	//drop data
			}

			slen = (ssize_t)snprintf(buf, buflen, "lxc.sysctl.%s", melem->object);
			if (slen >= buflen) {
				continue;	// buffer over -> drop data
			}

			bret = plxc->set_config_item(plxc, buf, melem->value);
			if (bret == false) {
				result = -1;
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"lxcutil: lxcutil_set_config_resource set config %s = %s fail.\n", buf, melem->value);
				#endif
				goto err_ret;
			}
		} else {
			; //nop
		}
	}

	return 0;

err_ret:

	return result;
}
/**
 * Create lxc config from container config fsconfig sub part.
 *
 * @param [in]	plxc	The lxc container instance to set config.
 * @param [in]	fsc		Pointer to container_fsconfig_t.
 * @return int
 * @retval 0	Success to set lxc config from fsc.
 * @retval -1	Got lxc error.
 * @retval -2	A bytes of config string is larger than buffer size. Critical case only.
 */
static int lxcutil_set_config_fs(struct lxc_container *plxc, container_fsconfig_t *fsc)
{
	int result = -1;
	bool bret = false;
	char buf[1024];
	ssize_t slen = 0, buflen = 0;
	container_fsmount_elem_t *melem = NULL;

	(void) memset(buf,0,sizeof(buf));

	dl_list_for_each(melem, &fsc->fsmount.mountlist, container_fsmount_elem_t, list) {
		buflen = (ssize_t)sizeof(buf) - 1;
		(void) memset(buf,0,sizeof(buf));
		if (melem->type == FSMOUNT_TYPE_FILESYSTEM) {
			if ((melem->from == NULL) || (melem->to == NULL) || (melem->fstype == NULL) || (melem->option == NULL)) {
				continue;	//drop data
			}

			slen = (ssize_t)snprintf(buf, buflen, "%s %s %s %s", melem->from, melem->to, melem->fstype, melem->option);
			if (slen >= buflen) {
				continue;	// buffer over -> drop data
			}

			bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
			if (bret == false) {
				result = -1;
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"lxcutil: lxcutil_set_config_fs set config %s = %s fail.\n", "lxc.mount.entry", buf);
				#endif
				goto err_ret;
			}
		} else if (melem->type == FSMOUNT_TYPE_DIRECTORY) {
			if ((melem->from == NULL) || (melem->to == NULL) || (melem->fstype == NULL) || (melem->option == NULL)) {
				continue;	//drop data
			}

			slen = (ssize_t)snprintf(buf, buflen, "%s %s %s %s", melem->from, melem->to, melem->fstype, melem->option);
			if (slen >= buflen) {
				continue;	// buffer over -> drop data
			}

			bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
			if (bret == false) {
				result = -1;
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"lxcutil: lxcutil_set_config_fs set config %s = %s fail.\n", "lxc.mount.entry", buf);
				#endif
				goto err_ret;
			}
		} else {
			//　FSMOUNT_TYPE_DELAYED is not set lxc config
			; //nop
		}
	}

	return 0;

err_ret:

	return result;
}
/**
 * Create lxc config from container config deviceconfig sub part for set default.
 *
 * @param [in]	plxc	The lxc container instance to set config.
 * @return int
 * @retval 0	Success to set lxc config from devc.
 * @retval -1	Got lxc error.
 * @retval -2	A bytes of config string is larger than buffer size. Critical case only.
 */
static int lxcutil_set_config_static_device_default(struct lxc_container *plxc)
{
	int result = 0;
	bool bret = false;

	// Set all devices are deny
	bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.deny", "a");
	if (bret == false) {
		result = -1;
		goto err_ret;
	}

	// /dev/null
	bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", "c 1:3 rwm");
	if (bret == false) {
		result = -1;
		goto err_ret;
	}

	// /dev/zero
	bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", "c 1:5 rwm");
	if (bret == false) {
		result = -1;
		goto err_ret;
	}

	// /dev/full
	bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", "c 1:7 rwm");
	if (bret == false) {
		result = -1;
		goto err_ret;
	}

	// /dev/tty
	bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", "c 5:0 rwm");
	if (bret == false) {
		result = -1;
		goto err_ret;
	}

	// /dev/ptmx
	bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", "c 5:2 rwm");
	if (bret == false) {
		result = -1;
		goto err_ret;
	}

	// /dev/random
	bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", "c 1:8 rwm");
	if (bret == false) {
		result = -1;
		goto err_ret;
	}

	// /dev/urandom
	bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", "c 1:9 rwm");
	if (bret == false) {
		result = -1;
		goto err_ret;
	}
	// /dev/pts/x
	bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", "c 136:* rwm");
	if (bret == false) {
		result = -1;
		goto err_ret;
	}

err_ret:

	return result;
}
/**
 * Create lxc config from container config deviceconfig sub part.
 *
 * @param [in]	plxc	The lxc container instance to set config.
 * @param [in]	devc	Pointer to container_deviceconfig_t.
 * @return int
 * @retval 0	Success to set lxc config from devc.
 * @retval -1	Got lxc error.
 * @retval -2	A bytes of config string is larger than buffer size. Critical case only.
 */
static int lxcutil_set_config_static_device(struct lxc_container *plxc, container_deviceconfig_t *devc)
{
	int result = -1, ret = -1;
	bool bret = false;
	char buf[1024];
	ssize_t slen = 0, buflen = 0;
	container_static_device_elem_t *develem = NULL;
	static const char sdevtype[2][2] = {"c","b"};
	const char *pdevtype = NULL;
	container_static_gpio_elem_t *gpioelem = NULL;
	container_static_iio_elem_t *iioelem = NULL;

	(void) memset(buf,0,sizeof(buf));

	if (devc->enable_protection == 1) {
		ret = lxcutil_set_config_static_device_default(plxc);
		if (ret < 0) {
			result = -1;
			goto err_ret;
		}
	}

	// static device node
	dl_list_for_each(develem, &devc->static_device.static_devlist, container_static_device_elem_t, list) {
		//device bind mount
		slen = 0;
		buflen = (ssize_t)sizeof(buf) - slen - 1;
		(void) memset(buf,0,sizeof(buf));

		if ((develem->from == NULL) || (develem->to == NULL)
			|| ((develem->optional == 0) && (develem->is_valid == 0))) {
			result = -2;
			goto err_ret;
		}

		if (develem->is_valid == 0) {
			continue;	//drop data
		}

		slen = (ssize_t)snprintf(buf, buflen, "%s %s none bind,rw", develem->from, develem->to);
		if (slen >= buflen) {
			continue;	// buffer over -> drop data
		}

		buflen = (ssize_t)sizeof(buf) - slen - 1;
		if (develem->optional == 1) {
			(void) strncpy(&buf[slen], ",optional", buflen);
			slen = slen + (ssize_t)sizeof(",optional") - 1;
		}

		buflen = (ssize_t)sizeof(buf) - slen - 1;
		if (develem->type == DEVICE_TYPE_DEVNODE) {
			(void) strncpy(&buf[slen], ",create=file", buflen);
			slen = slen + (ssize_t)sizeof(",create=file") - 1;
		} if (develem->type == DEVICE_TYPE_DEVDIR) {
			(void) strncpy(&buf[slen], ",create=dir", buflen);
			slen = slen + (ssize_t)sizeof(",create=dir") - 1;
		}

		bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
		if (bret == false) {
			result = -1;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
			#endif
			goto err_ret;
		}

		// device allow
		buflen = (ssize_t)sizeof(buf) - 1;
		(void) memset(buf,0,sizeof(buf));

		if (develem->devtype == DEVNODE_TYPE_BLK) {
			pdevtype = sdevtype[1];
		} else {
			pdevtype = sdevtype[0];	//Not block = char
		}

		if (develem->wideallow == 1) {
			// allow all minor
			slen = (ssize_t)snprintf(buf, buflen, "%s %d:* rw", pdevtype, develem->major); // static node is block to mknod
			if (slen >= buflen) {
				continue;	// buffer over -> drop data
			}
		} else {
			slen = (ssize_t)snprintf(buf, buflen, "%s %d:%d rw", pdevtype, develem->major, develem->minor); // static node is block to mknod
			if (slen >= buflen) {
				continue;	// buffer over -> drop data
			}
		}

		bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", buf);
	}

	// gpio
	dl_list_for_each(gpioelem, &devc->static_device.static_gpiolist, container_static_gpio_elem_t, list) {
		//device bind mount
		slen = 0;
		buflen = (ssize_t)sizeof(buf) - slen - 1;
		(void) memset(buf,0,sizeof(buf));

		if ((gpioelem->from == NULL) || (gpioelem->to == NULL) || (develem->is_valid == 0)) {
			// gpio is mandatory device
			result = -2;
			goto err_ret;
		}

		slen = (ssize_t)snprintf(buf, buflen, "%s %s none bind", gpioelem->from, gpioelem->to);
		if (slen >= buflen) {
			continue;	// buffer over -> drop data
		}

		buflen = (ssize_t)sizeof(buf) - slen - 1;
		if (devgpio_direction_isvalid(gpioelem->portdirection) == 1) {
			// in = read only = not need bind mount
			// out, low, high = need to rw bind mount to set gpio value
			// not set = shall set direction in guest = rw mount
			if (gpioelem->portdirection != DEVGPIO_DIRECTION_IN) {
				(void)strncpy(&buf[slen], ",rw", buflen);
				slen = slen + (ssize_t)sizeof(",rw") - 1;

				bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
					#endif
					goto err_ret;
				}
			}
		} else {
			// skip port
			continue;
		}
		// device allow is not need
	}

	// iio
	dl_list_for_each(iioelem, &devc->static_device.static_iiolist, container_static_iio_elem_t, list) {
		//device bind mount
		slen = 0;
		buflen = (ssize_t)sizeof(buf) - slen - 1;
		buf[0] = '\0';

		if ((iioelem->sysfrom == NULL) || (iioelem->systo == NULL)) {
			// iio(sysfs) parameter was broken
			result = -2;
			goto err_ret;
		}

		if (iioelem->is_sys_valid == 0) {
			if(iioelem->optional == 1) {
				// If device is optional, no device case is skip
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base skip  %s\n", iioelem->sysfrom);
				#endif
				continue;
			} else {
				// Not optional, error.
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base not valid sys %s - error\n", iioelem->sysfrom);
				#endif
				result = -2;
				goto err_ret;
			}
		}

		slen = (ssize_t)snprintf(buf, buflen, "%s %s none bind,rw", iioelem->sysfrom, iioelem->systo);
		if (slen >= buflen) {
			continue;	// buffer over -> drop data
		}

		bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
		if (bret == false) {
			result = -1;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
			#endif
			goto err_ret;
		}
		// sysfs part end.

		if ((iioelem->devfrom != NULL) && (iioelem->devto != NULL) && (iioelem->devnode != NULL)) {
			// dev node part required.
			slen = 0;
			buflen = (ssize_t)sizeof(buf) - slen - 1;
			buf[0] = '\0';

			if (iioelem->is_dev_valid == 1) {
				slen = (ssize_t)snprintf(buf, buflen, "%s %s none bind,rw", iioelem->devfrom, iioelem->devto);
				if (slen >= buflen) {
					continue;	// buffer over -> drop data
				}

				buflen = (ssize_t)sizeof(buf) - slen - 1;
				if (iioelem->optional == 1) {
					(void)strncpy(&buf[slen], ",optional", buflen);
					slen = slen + (ssize_t)sizeof(",optional") - 1;
				}

				buflen = (ssize_t)sizeof(buf) - slen - 1;
				(void)strncpy(&buf[slen], ",create=file", buflen);
				slen = slen + (ssize_t)sizeof(",create=file") - 1;

				bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
					#endif
					goto err_ret;
				}

				// device allow
				buflen = (ssize_t)sizeof(buf) - 1;
				buf[0] = '\0';

				slen = (ssize_t)snprintf(buf, buflen, "c %d:%d rw", iioelem->major, iioelem->minor); // static node is block to mknod
				if (slen >= buflen) {
					continue;	// buffer over -> drop data
				}

				bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", buf);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
					#endif
					goto err_ret;
				}
			} else {
				if(iioelem->optional == 0) {
					// Not optional, error.
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base not valid dev %s - error\n", iioelem->devfrom);
					#endif
					result = -2;
					goto err_ret;
				}
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base skip  %s\n", iioelem->devfrom);
				#endif
			}
		}
	}

	return 0;

err_ret:

	return result;
}
/**
 * Create lxc config from container config netifconfig sub part.
 *
 * @param [in]	plxc	The lxc container instance to set config.
 * @param [in]	netc	Pointer to container_netifconfig_t.
 * @return int
 * @retval 0	Success to set lxc config from netc.
 * @retval -1	Got lxc error.
 * @retval -2	A bytes of config string is larger than buffer size. Critical case only.
 */
static int lxcutil_set_config_static_netif(struct lxc_container *plxc, container_netifconfig_t *netc)
{
	int result = -1;
	bool bret = false;
	char buf[1024];
	container_static_netif_elem_t *netelem = NULL;
	int num = 0;

	(void) memset(buf,0,sizeof(buf));

	// static net if
	dl_list_for_each(netelem, &netc->static_netif.static_netiflist, container_static_netif_elem_t, list) {
		buf[0] = '\0';

		// veth support
		if (netelem->type == STATICNETIF_VETH) {
			netif_elem_veth_t *veth = (netif_elem_veth_t*)netelem->setting;

			(void)snprintf(buf, sizeof(buf), "lxc.net.%d.type", num);	//No issue for buffer length.
			bret = plxc->set_config_item(plxc, buf, "veth");
			if (bret == false) {
				result = -1;
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, "veth");
				#endif
				goto err_ret;
			}

			// name is optional, lxc default is ethX.
			if (veth->name != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.name", num);	//No issue for buffer length.
				bret = plxc->set_config_item(plxc, buf, veth->name);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->name);
					#endif
					goto err_ret;
				}
			}

			// link - linking bridge device - is optional, lxc default is not linking.
			if (veth->link != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.link", num);	//No issue for buffer length.
				bret = plxc->set_config_item(plxc, buf, veth->link);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->link);
					#endif
					goto err_ret;
				}
			}

			// flags is optional, lxc default is link down.
			if (veth->flags != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.flags", num);	//No issue for buffer length.
				bret = plxc->set_config_item(plxc, buf, veth->flags);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->flags);
					#endif
					goto err_ret;
				}
			}

			// hwaddr is optional, lxc default is random mac address.
			if (veth->hwaddr != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.hwaddr", num);	//No issue for buffer length.
				bret = plxc->set_config_item(plxc, buf, veth->hwaddr);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->hwaddr);
					#endif
					goto err_ret;
				}
			}

			// mode is optional, lxc default is bridge mode.
			if (veth->mode != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.veth.mode", num);	//No issue for buffer length.
				bret = plxc->set_config_item(plxc, buf, veth->mode);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->mode);
					#endif
					goto err_ret;
				}
			}

			// address is optional, lxc default is not set ip address.
			if (veth->address != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.ipv4.address", num);	//No issue for buffer length.
				bret = plxc->set_config_item(plxc, buf, veth->address);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->address);
					#endif
					goto err_ret;
				}
			}

			// gateway is optional, lxc default is not set default gateway.
			if (veth->gateway != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.ipv4.gateway", num);	//No issue for buffer length.
				bret = plxc->set_config_item(plxc, buf, veth->gateway);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->gateway);
					#endif
					goto err_ret;
				}
			}
		}
	}

	return 0;

err_ret:

	return result;
}


/**
 * Create lxc container instance and set to runtime data of container_config_t.
 *
 * @param [in]	cc	Pointer to container_config_t.
 * @return int
 * @retval 0	Success to create lxc container instance.
 * @retval -1	Got lxc error.
 */
int lxcutil_create_instance(container_config_t *cc)
{
	struct lxc_container *plxc;
	int ret = 1;
	int result = -1;
	bool bret = false;

	// Setup container struct
	plxc = lxc_container_new(cc->name, NULL);
	if (plxc == NULL) {
		result = -2;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_create_instance container %s create fail.\n",cc->name);
		#endif
		goto err_ret;
	}

	plxc->clear_config(plxc);

	bret = plxc->set_config_item(plxc, "lxc.uts.name", cc->name);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_create_instance set config %s = %s fail.\n", "lxc.uts.name", cc->name);
		#endif
		goto err_ret;
	}

	ret = lxcutil_set_config_base(plxc, &cc->baseconfig);
	if (ret < 0) {
		result = -1;
		goto err_ret;
	}

	ret = lxcutil_set_config_resource(plxc, &cc->resourceconfig);
	if (ret < 0) {
		result = -1;
		goto err_ret;
	}

	ret = lxcutil_set_config_fs(plxc, &cc->fsconfig);
	if (ret < 0) {
		result = -1;
		goto err_ret;
	}

	ret = lxcutil_set_config_static_device(plxc, &cc->deviceconfig);
	if (ret < 0) {
		result = -1;
		goto err_ret;
	}

	ret = lxcutil_set_config_static_netif(plxc, &cc->netifconfig);
	if (ret < 0) {
		result = -1;
		goto err_ret;
	}

	bret = plxc->want_daemonize(plxc, true);
	if (bret == false) {
		result = -1;
		goto err_ret;
	}

	cc->runtime_stat.lxc = plxc;
	cc->runtime_stat.pid = -1;

	#ifdef PRINTF_DEBUG_CONFIG_OUT
	{
		char buf[1024];

		(void) snprintf(buf, sizeof(buf)-1u, "/tmp/dbgcfg-%s.txt", cc->name);
		bret = plxc->save_config(cc->runtime_stat.lxc, buf);
		if (bret == false) {
			(void) fprintf(stdout,"lxcutil: save_config fail.\n");
		}

	}
	#endif

	return 0;

err_ret:
	if (plxc != NULL) {
		(void) lxc_container_put(plxc);
	}

	return result;
}
