/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	lxc-util.c
 * @brief	LXC control interface for container manager use.
 */

#include "lxc-util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <lxc/lxccontainer.h>

#include "cm-utils.h"
#include "uevent_injection.h"

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
	int buflen = 0, slen = 0;

	(void) memset(buf,0,sizeof(buf));

	// rootfs
	slen = snprintf(buf, (sizeof(buf)-1), "dir:%s", bc->rootfs.path);
	if (slen >= (sizeof(buf)-1)) {
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
			buflen = sizeof(buf) -1;

			if (exdisk->mode == DISKMOUNT_TYPE_RW) {
				slen = snprintf(buf, buflen, "%s %s none bind,rw,create=dir", exdisk->from, exdisk->to);
			} else {
				slen = snprintf(buf, buflen, "%s %s none bind,ro,create=dir", exdisk->from, exdisk->to);
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
		if (strlen(bc->cap.drop) > 0){
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
		if (strlen(bc->cap.keep) > 0){
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
		if (ret >= sizeof(buf)) {
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
		if (ret >= sizeof(buf)) {
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

	// static setting
	bret = plxc->set_config_item(plxc, "lxc.tty.max", "1");
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s fail.\n", "lxc.tty.max");
		#endif
		goto err_ret;
	}

	bret = plxc->set_config_item(plxc, "lxc.pty.max", "1");
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
	int slen = 0, buflen = 0;
	container_resource_elem_t *melem = NULL;

	(void) memset(buf,0,sizeof(buf));

	dl_list_for_each(melem, &rsc->resource.resourcelist, container_resource_elem_t, list) {
		buflen = sizeof(buf) - 1;
		buf[0] = '\0';

		if (melem->type == RESOURCE_TYPE_CGROUP) {
			if (melem->object == NULL || melem->value == NULL) {
				continue;	//drop data
			}

			slen = snprintf(buf, buflen, "lxc.cgroup.%s", melem->object);
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

			slen = snprintf(buf, buflen, "lxc.prlimit.%s", melem->object);
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

			slen = snprintf(buf, buflen, "lxc.sysctl.%s", melem->object);
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
	int slen = 0, buflen = 0;
	container_fsmount_elem_t *melem = NULL;

	(void) memset(buf,0,sizeof(buf));

	// static settings
	bret = plxc->set_config_item(plxc, "lxc.mount.auto", "cgroup:mixed proc:mixed sys:mixed");
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_fs set config %s = %s fail.\n", "lxc.mount.auto", "cgroup:mixed proc:mixed sys:mixed");
		#endif
		goto err_ret;
	}

	dl_list_for_each(melem, &fsc->fsmount.mountlist, container_fsmount_elem_t, list) {
		buflen = sizeof(buf) - 1;
		(void) memset(buf,0,sizeof(buf));
		if (melem->type == FSMOUNT_TYPE_FILESYSTEM) {
			if (melem->from == NULL || melem->to == NULL || melem->fstype == NULL || melem->option == NULL) {
				continue;	//drop data
			}

			slen = snprintf(buf, buflen, "%s %s %s %s", melem->from, melem->to, melem->fstype, melem->option);
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
			if (melem->from == NULL || melem->to == NULL || melem->fstype == NULL || melem->option == NULL) {
				continue;	//drop data
			}

			slen = snprintf(buf, buflen, "%s %s %s %s", melem->from, melem->to, melem->fstype, melem->option);
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
			; //nop
		}
	}

	return 0;

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
	int result = -1;
	bool bret = false;
	char buf[1024];
	int slen = 0, buflen = 0;
	container_static_device_elem_t *develem = NULL;
	static const char sdevtype[2][2] = {"c","b"};
	const char *pdevtype = NULL;
	container_static_gpio_elem_t *gpioelem = NULL;
	container_static_iio_elem_t *iioelem = NULL;

	(void) memset(buf,0,sizeof(buf));

	// static device node
	dl_list_for_each(develem, &devc->static_device.static_devlist, container_static_device_elem_t, list) {
		//device bind mount
		slen = 0;
		buflen = sizeof(buf) - slen - 1;
		(void) memset(buf,0,sizeof(buf));

		if (develem->from == NULL || develem->to == NULL
			|| (develem->optional == 0 && develem->is_valid == 0)) {
			result = -2;
			goto err_ret;
		}

		if (develem->is_valid == 0) {
			continue;	//drop data
		}

		slen = snprintf(buf, buflen, "%s %s none bind,rw", develem->from, develem->to);
		if (slen >= buflen) {
			continue;	// buffer over -> drop data
		}

		buflen = sizeof(buf) - slen - 1;
		if (develem->optional == 1) {
			(void)strncpy(&buf[slen], ",optional", buflen);
			slen = slen + sizeof(",optional") - 1;
		}

		buflen = sizeof(buf) - slen - 1;
		if (develem->type == DEVICE_TYPE_DEVNODE) {
			(void)strncpy(&buf[slen], ",create=file", buflen);
			slen = slen + sizeof(",create=file") - 1;
		} if (develem->type == DEVICE_TYPE_DEVDIR) {
			(void)strncpy(&buf[slen], ",create=dir", buflen);
			slen = slen + sizeof(",create=dir") - 1;
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
		buflen = sizeof(buf) - 1;
		(void) memset(buf,0,sizeof(buf));

		if (develem->devtype == DEVNODE_TYPE_BLK) {
			pdevtype = sdevtype[1];
		}
		else {
			pdevtype = sdevtype[0];	//Not block = char
		}

		if (develem->wideallow == 1) {
			// allow all minor
			slen = snprintf(buf, buflen, "%s %d:* rw", pdevtype, develem->major); // static node is block to mknod
			if (slen >= buflen) {
				continue;	// buffer over -> drop data
			}
		} else {
			slen = snprintf(buf, buflen, "%s %d:%d rw", pdevtype, develem->major, develem->minor); // static node is block to mknod
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
		buflen = sizeof(buf) - slen - 1;
		(void) memset(buf,0,sizeof(buf));

		if (gpioelem->from == NULL || gpioelem->to == NULL || develem->is_valid == 0) {
			// gpio is mandatory device
			result = -2;
			goto err_ret;
		}

		slen = snprintf(buf, buflen, "%s %s none bind", gpioelem->from, gpioelem->to);
		if (slen >= buflen) {
			continue;	// buffer over -> drop data
		}

		buflen = sizeof(buf) - slen - 1;
		if (devgpio_direction_isvalid(gpioelem->portdirection) == 1) {
			// in = read only = not need bind mount
			// out, low, high = need to rw bind mount to set gpio value
			// not set = shall set direction in guest = rw mount
			if (gpioelem->portdirection != DEVGPIO_DIRECTION_IN) {
				(void)strncpy(&buf[slen], ",rw", buflen);
				slen = slen + sizeof(",rw") - 1;

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
		buflen = sizeof(buf) - slen - 1;
		buf[0] = '\0';

		if (iioelem->sysfrom == NULL || iioelem->systo == NULL ) {
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

		slen = snprintf(buf, buflen, "%s %s none bind,rw", iioelem->sysfrom, iioelem->systo);
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

		if (iioelem->devfrom != NULL && iioelem->devto != NULL && iioelem->devnode != NULL) {
			// dev node part required.
			slen = 0;
			buflen = sizeof(buf) - slen - 1;
			buf[0] = '\0';

			if (iioelem->is_dev_valid == 1) {
				slen = snprintf(buf, buflen, "%s %s none bind,rw", iioelem->devfrom, iioelem->devto);
				if (slen >= buflen) {
					continue;	// buffer over -> drop data
				}

				buflen = sizeof(buf) - slen - 1;
				if (iioelem->optional == 1) {
					(void)strncpy(&buf[slen], ",optional", buflen);
					slen = slen + sizeof(",optional") - 1;
				}

				buflen = sizeof(buf) - slen - 1;
				(void)strncpy(&buf[slen], ",create=file", buflen);
				slen = slen + sizeof(",create=file") - 1;

				bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
					#endif
					goto err_ret;
				}

				// device allow
				buflen = sizeof(buf) - 1;
				buf[0] = '\0';

				slen = snprintf(buf, buflen, "c %d:%d rw", iioelem->major, iioelem->minor); // static node is block to mknod
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

	#ifdef _PRINTF_DEBUG_
	{
		char buf[1024];

		snprintf(buf, sizeof(buf)-1, "/tmp/dbgcfg-%s.txt", cc->name);
		bret = plxc->save_config(cc->runtime_stat.lxc, buf);
		if (bret == false) {
			(void) fprintf(stdout,"lxcutil: save_config fail.\n");
		}

	}
	#endif

	return 0;

err_ret:
	if (plxc != NULL) {
		(void)lxc_container_put(plxc);
	}

	return result;
}
/**
 * Guest container shutdown by lxc shutdown.
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0	Success to request lxc container shutdown.
 * @retval -1	Got lxc error.
 */
int lxcutil_container_shutdown(container_config_t *cc)
{
	bool bret = false;
	int result = -1;

	if (cc->runtime_stat.lxc != NULL) {
		bret = cc->runtime_stat.lxc->shutdown(cc->runtime_stat.lxc, 0);	//non block shutdown
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout, "lxcutil_container_shutdown: shutdown request to guest %s\n", cc->name);
		#endif
	}

	if (bret == true) {
		result = 0;
	}

	return result;
}
/**
 * Guest container kill (Send SIGKILL).
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0	Success to send SIGKILL to lxc container.
 * @retval -1	Got lxc error.
 */
int lxcutil_container_forcekill(container_config_t *cc)
{
	pid_t pid = -1;

	if (cc->runtime_stat.lxc != NULL) {
		pid = lxcutil_get_init_pid(cc);

		if (pid > 0) {
			(void) kill(pid, SIGKILL);
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "lxcutil_container_forcekill: kill signal send to guest %s\n", cc->name );
			#endif
		}
	}

	return 0;
}
/**
 * Release lxc instance in container_config_t.
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0	Success to remove lxc container instance.
 * @retval -1	Got lxc error.
 */
int lxcutil_release_instance(container_config_t *cc)
{

	if (cc->runtime_stat.lxc != NULL) {
		(void)lxc_container_put(cc->runtime_stat.lxc);
	}

	cc->runtime_stat.lxc = NULL;
	cc->runtime_stat.pid = -1;

	return 0;
}
/**
 * Get pid of guest init by container_config_t.
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0<	Success get pid of guest init.
 * @retval -1	Got lxc error.
 */
pid_t lxcutil_get_init_pid(container_config_t *cc)
{
	pid_t target_pid = -1;

	if (cc->runtime_stat.lxc != NULL) {
		if (cc->runtime_stat.pid <= 0) {
			target_pid = cc->runtime_stat.lxc->init_pid(cc->runtime_stat.lxc);
			if (target_pid > 0) {
				cc->runtime_stat.pid = target_pid;
			}
		} else {
			target_pid = cc->runtime_stat.pid;
		}
	}

	return target_pid;
}
/**
 * Add or remove device node in guest container.
 * This function is sub function for lxcutil_dynamic_device_add_to_guest.
 * This function exec in child process side after fork.
 *
 * @param [in]	target_pid	A pid of guest container init.
 * @param [in]	path		The path for device node in guest.
 * @param [in]	is_add		Set add or remove. (1=add, 0=remove)
 * @param [in]	devmode		File permission for guest device node.
 * @param [in]	devnum		Device major/minor number for target device.
 * @return int
 * @retval 0	Success to operations.
 * @retval -1	Critical error.
 */
static int lxcutil_add_remove_guest_node_child(pid_t target_pid, const char *path, int is_add, mode_t devmode, dev_t devnum)
{
	int ret = -1;
	char buf[PATH_MAX];

	(void) memset(buf, 0 , sizeof(buf));

	ret = snprintf(buf, sizeof(buf), "/proc/%d/root", target_pid);
	if (!(ret < sizeof(buf)-1)) {
		return -1;
	}

	ret = chroot(buf);
	if (ret < 0) {
		return -1;
	}

	ret = chdir("/");
	if (ret < 0) {
		return -1;
	}

	(void) unlink(path);
	if (is_add != 1) {
		return 0;
	}

	(void) strncpy(buf, path, sizeof(buf)-1);

	ret = mkdir_p(buf, 0755);
	if (ret < 0) {
		return -1;
	}

	/* create the device node */
	ret = mknod(path, devmode, devnum);
	if (ret < 0 && errno != EEXIST) {
		return -1;
	}

	return 0;
}
/**
 * Add or remove device node in guest container.
 * This function is sub function for lxcutil_dynamic_device_add_to_guest.
 * This function exec in parent process side.
 *
 * @param [in]	target_pid	A pid of guest container init.
 * @param [in]	path		The path for device node in guest.
 * @param [in]	is_add		Set add or remove. (1=add, 0=remove)
 * @param [in]	devnum		Device major/minor number for target device.
 * @return int
 * @retval 0	Success to operations.
 * @retval -1	Critical error.
 */
static int lxcutil_add_remove_guest_node(pid_t target_pid, const char *path, int is_add, dev_t devnum)
{
	int ret = -1;
	pid_t child_pid = -1;
	struct stat sb = {0};
	mode_t devmode = 0;;

	if (is_add == 1) {
		ret = stat(path, &sb);
		if (ret < 0) {
			return -1;
		}

		devmode = sb.st_mode;
	}

	child_pid = fork();
	if (child_pid < 0) {
		return -3;
	}

	if (child_pid == 0) {
		// run on child process, must be exit.
		ret = lxcutil_add_remove_guest_node_child(target_pid, path, is_add, devmode, devnum);
		if (ret < 0)
			_exit(EXIT_FAILURE);

		_exit(EXIT_SUCCESS);
	}

	ret = wait_child_pid(child_pid);
	if (ret < 0) {
		return -2;
	}

	return 0;
}
/**
 * The function of dynamic device operation.
 * Device allow/deny setting by cgroup.
 * Device node creation and remove.
 *
 * @param [in]	cc		Pointer to container_config_t of target container.
 * @param [in]	lddr	The path for device node in guest.
 * @return int
 * @retval 0	Success to operations.
 * @retval -1	Critical error.
 */
int lxcutil_dynamic_device_operation(container_config_t *cc, lxcutil_dynamic_device_request_t *lddr)
{
	bool bret = false;
	int ret = -1;
	int result = -1;

	if (cc->runtime_stat.lxc == NULL) {
		result = -1;
		goto err_ret;
	}

	if (lddr->devtype == DEVNODE_TYPE_NET) {
		// This operation is not support network device.
		result = -1;
		goto err_ret;
	}

	if (lddr->dev_major < 0 || lddr->dev_minor < 0) {
		return 0;	// No need to allow/deny device by cgroup
	}

	// Device allow/deny setting by cgroup.
	if (lddr->is_allow_device == 1) {
		const char *permission = NULL;
		const char perm_default[] = "rw";
		char buf[1024];

		permission = lddr->permission;
		if (permission == NULL) {
			permission = perm_default;
		}

		if (lddr->devtype == DEVNODE_TYPE_BLK) {
			ret = snprintf(buf, sizeof(buf), "b %d:%d %s", lddr->dev_major, lddr->dev_minor, permission);
		} else {
			ret = snprintf(buf, sizeof(buf), "c %d:%d %s", lddr->dev_major, lddr->dev_minor, permission);
		}

		if (!(ret < (sizeof(buf)-1))) {
			result = -1;
			goto err_ret;
		}

		if (lddr->operation == DCD_UEVENT_ACTION_ADD) {
			bret = cc->runtime_stat.lxc->set_cgroup_item(cc->runtime_stat.lxc, "devices.allow", buf);
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "lxc set_cgroup_item: %s = %s\n", "devices.allow", buf);
			#endif
		} else if (lddr->operation == DCD_UEVENT_ACTION_REMOVE) {
			bret = cc->runtime_stat.lxc->set_cgroup_item(cc->runtime_stat.lxc, "devices.deny", buf);
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "lxc set_cgroup_item: %s = %s\n", "devices.deny", buf);
			#endif
		}
		if (bret == false) {
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "lxcutil_dynamic_device_operation: fail set_cgroup_item %s\n", buf);
			#endif
			result = -2;
			goto err_ret;
		}
	}

	// Device node creation and remove.
	if (lddr->is_create_node == 1) {
		dev_t devnum = 0;
		ret = -1;
		pid_t target_pid = 0;

		target_pid = lxcutil_get_init_pid(cc);

		devnum = makedev(lddr->dev_major, lddr->dev_minor);
		if (lddr->operation == DCD_UEVENT_ACTION_ADD) {
			ret = lxcutil_add_remove_guest_node(target_pid, lddr->devnode, 1, devnum);
		} else if (lddr->operation == DCD_UEVENT_ACTION_REMOVE) {
			ret = lxcutil_add_remove_guest_node(target_pid, lddr->devnode, 0, devnum);
		}
		if (ret < 0) {
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "lxcutil_dynamic_device_operation: fail lxcutil_add_remove_guest_node (%d) %s\n", ret, lddr->devnode);
			#endif
			result = -1;
			goto err_ret;
		}
	}

	return 0;

err_ret:

	return result;
}
/**
 * Add network inteface to guest container.
 * This function attach to guest container using lxc attach_interface.
 *
 * @param [in]	cc		Pointer to container_config_t.
 * @param [in]	cdne	Pointer to container_dynamic_netif_elem_t, that include target network interface data.
 * @return int
 * @retval 0	Success to operations.
 * @retval -1	Got lxc error.
 */
int lxcutil_dynamic_networkif_add_to_guest(container_config_t *cc, container_dynamic_netif_elem_t *cdne)
{
	bool bret = false;
	int result = -1;

	if (cc->runtime_stat.lxc != NULL) {
		bret = cc->runtime_stat.lxc->attach_interface(cc->runtime_stat.lxc, cdne->ifname, cdne->ifname);
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout, "lxcutil_dynamic_networkif_add_to_guest: network interface %s assign to %s\n", cdne->ifname, cc->name );
		#endif
	}

	if (bret == true) {
		result = 0;
	}

	return result;
}
