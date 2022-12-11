/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cluster-service.c
 * @brief	main source file for cluster-service
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

#include "cm-utils.h"
#include "uevent_injection.h"

/**
 * Read json string with memory alocation
 *
 * @param [in]	file		Full file path for json file
 * @return int
 * @retval -1 Json file error.
 * @retval -2 Json file perse error. 
 * @retval -3 Memory allocation error. 
 */
static int lxcutil_set_config_base(struct lxc_container *plxc, container_baseconfig_t *bc)
{
	int ret = 1;
	int result = -1;
	bool bret = false;
	char buf[1024];
	int buflen = 0, slen = 0;

	memset(buf,0,sizeof(buf));

	// rootfs
	(void)strncat(buf, "dir:", sizeof(buf)-1);
	(void)strncat(buf, bc->rootfs.path, sizeof(buf)-1-4);
	
	bret = plxc->set_config_item(plxc, "lxc.rootfs.path", buf);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.rootfs.path", buf);
		#endif
		goto err_ret;
	}

	// extradisk - optional
	if (!dl_list_empty(&bc->extradisk_list)) {
		container_baseconfig_extradisk_t *exdisk = NULL;

		dl_list_for_each(exdisk, &bc->extradisk_list, container_baseconfig_extradisk_t, list) {
			memset(buf, 0, sizeof(buf));
			buflen = sizeof(buf) -1;

			if (exdisk->mode == DISKMOUNT_TYPE_RW) {
				slen = snprintf(buf, buflen, "%s %s none bind,rw,create=dir", exdisk->from, exdisk->to); 
			} else {
				slen = snprintf(buf, buflen, "%s %s none bind,ro,create=dir", exdisk->from, exdisk->to); 
			}

			if (slen != buflen) {
				bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
					#endif
					goto err_ret;
				}
			}
		}
	}

	// signal - mandatoly
	bret = plxc->set_config_item(plxc, "lxc.signal.halt", bc->lifecycle.halt);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.signal.halt", bc->lifecycle.halt);
		#endif
		goto err_ret;
	}

	bret = plxc->set_config_item(plxc, "lxc.signal.reboot", bc->lifecycle.reboot);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.signal.reboot", bc->lifecycle.reboot);
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
				fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.cap.drop", bc->cap.drop);
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
				fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.cap.keep", bc->cap.keep);
				#endif
				goto err_ret;
			}
		}
	}

	// idmap - optional
	if (bc->idmaps.enabled == 1) {
		memset(buf,0,sizeof(buf));
		ret = snprintf(buf,sizeof(buf),"u %d %d %d", bc->idmaps.uid.guest_root_id, bc->idmaps.uid.host_start_id, bc->idmaps.uid.num_of_id);
		if (ret >= sizeof(buf)) {
			result = -2;
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"lxcutil: lxcutil_set_config_base idmap (uid) too long parameter.\n");
			#endif
			goto err_ret;
		}

		bret = plxc->set_config_item(plxc, "lxc.idmap", buf);
		if (bret == false) {
			result = -1;
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.idmap", buf);
			#endif
			goto err_ret;
		}

		memset(buf,0,sizeof(buf));
		ret = snprintf(buf,sizeof(buf),"g %d %d %d", bc->idmaps.gid.guest_root_id, bc->idmaps.gid.host_start_id, bc->idmaps.gid.num_of_id);
		if (ret >= sizeof(buf)) {
			result = -2;
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"lxcutil: lxcutil_set_config_base idmap (gid) too long parameter.\n");
			#endif
			goto err_ret;
		}

		bret = plxc->set_config_item(plxc, "lxc.idmap", buf);
		if (bret == false) {
			result = -1;
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.idmap", buf);
			#endif
			goto err_ret;
		}

	}
	#ifdef _PRINTF_DEBUG_
	else {
		fprintf(stderr,"lxcutil: lxcutil_set_config_base idmap is disabled (%d)\n", bc->idmaps.enabled);
	}
	#endif


	// static setting
	bret = plxc->set_config_item(plxc, "lxc.tty.max", "1");
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s fail.\n", "lxc.tty.max");
		#endif
		goto err_ret;
	}

	bret = plxc->set_config_item(plxc, "lxc.pty.max", "1");
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s fail.\n", "lxc.tty.max");
		#endif
		goto err_ret;
	}

	return 0;

err_ret:
	
	return result;
}

/**
 * Read json string with memory alocation
 *
 * @param [in]	file		Full file path for json file
 * @return int
 * @retval -1 lxc runtime error.
 * @retval -2 mandatory setting error. 
 * @retval -3 TODO. 
 */
static int lxcutil_set_config_fs(struct lxc_container *plxc, container_fsconfig_t *fsc)
{
	int ret = 1;
	int result = -1;
	bool bret = false;
	char buf[1024];
	int slen = 0, buflen = 0;
	container_fsmount_elem_t *melem = NULL;

	memset(buf,0,sizeof(buf));

	// static settings
	bret = plxc->set_config_item(plxc, "lxc.mount.auto", "cgroup:mixed proc:mixed sys:mixed");
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"lxcutil: lxcutil_set_config_fs set config %s = %s fail.\n", "lxc.mount.auto", "cgroup:mixed proc:mixed sys:mixed");
		#endif
		goto err_ret;
	}

	dl_list_for_each(melem, &fsc->fsmount.mountlist, container_fsmount_elem_t, list) {
		buflen = sizeof(buf) - 1;
		memset(buf,0,sizeof(buf));
		if (melem->type == FSMOUNT_TYPE_FILESYSTEM) {
			if (melem->from == NULL || melem->to == NULL || melem->fstype == NULL || melem->option == NULL)
				continue;	//drop data

			slen = snprintf(buf, buflen, "%s %s %s %s", melem->from, melem->to, melem->fstype, melem->option); 
			if (slen == buflen)
				continue;	// buffer over -> drop data
			
			bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
			if (bret == false) {
				result = -1;
				#ifdef _PRINTF_DEBUG_
				fprintf(stderr,"lxcutil: lxcutil_set_config_fs set config %s = %s fail.\n", "lxc.mount.entry", buf);
				#endif
				goto err_ret;
			}
		} else if (melem->type == FSMOUNT_TYPE_DIRECTRY) {
			if (melem->from == NULL || melem->to == NULL || melem->fstype == NULL || melem->option == NULL)
				continue;	//drop data

			slen = snprintf(buf, buflen, "%s %s %s %s", melem->from, melem->to, melem->fstype, melem->option); 
			if (slen == buflen)
				continue;	// buffer over -> drop data
			
			bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
			if (bret == false) {
				result = -1;
				#ifdef _PRINTF_DEBUG_
				fprintf(stderr,"lxcutil: lxcutil_set_config_fs set config %s = %s fail.\n", "lxc.mount.entry", buf);
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
 * Read json string with memory alocation
 *
 * @param [in]	file		Full file path for json file
 * @return int
 * @retval -1 lxc runtime error.
 * @retval -2 mandatory setting error. 
 * @retval -3 TODO. 
 */
static int lxcutil_set_config_static_device(struct lxc_container *plxc, container_deviceconfig_t *devc)
{
	int ret = 1;
	int result = -1;
	bool bret = false;
	char buf[1024];
	int slen = 0, buflen = 0;
	container_static_device_elem_t *develem = NULL;
	static const char sdevtype[2][2] = {"c","b"};
	const char *pdevtype = NULL;
	container_static_gpio_elem_t *gpioelem = NULL;
	container_static_iio_elem_t *iioelem = NULL;


	memset(buf,0,sizeof(buf));

	// static device node
	dl_list_for_each(develem, &devc->static_device.static_devlist, container_static_device_elem_t, list) {
		//device bind mount
		buflen = sizeof(buf) - 1;
		memset(buf,0,sizeof(buf));

		if (develem->from == NULL || develem->to == NULL 
			|| (develem->optional == 0 && develem->is_valid == 0)) {
			result = -2;
			goto err_ret;
		}

		if (develem->is_valid == 0)
			continue;	//drop data

		slen = snprintf(buf, buflen, "%s %s none bind,rw", develem->from, develem->to); 
		if (slen == buflen)
			continue;	// buffer over -> drop data

		buflen = buflen - slen;
		if (develem->optional == 1) {
			(void)strncat(buf, ",optional", buflen);
			slen = sizeof(",optional");
		} else
			slen = 0;

		buflen = buflen - slen;
		if (develem->type == DEVICE_TYPE_DEVNODE) {
			(void)strncat(buf, ",create=file", buflen);
			slen = sizeof(",create=dir");
		} if (develem->type == DEVICE_TYPE_DEVDIR) {
			(void)strncat(buf, ",create=dir", buflen);
			slen = sizeof(",create=dir");
		} else
			slen = 0;

		bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
		if (bret == false) {
			result = -1;
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
			#endif
			goto err_ret;
		}

		// device allow
		buflen = sizeof(buf) - 1;
		memset(buf,0,sizeof(buf));

		if (develem->devtype == DEVNODE_TYPE_BLK)
			pdevtype = sdevtype[1];
		else
			pdevtype = sdevtype[0];	//Not block = char

		if (develem->wideallow == 1) {
			// allow all minor
			slen = snprintf(buf, buflen, "%s %d:* rw", pdevtype, develem->major); // static node is block to mknod
			if (slen == buflen)
				continue;	// buffer over -> drop data
		} else {
			slen = snprintf(buf, buflen, "%s %d:%d rw", pdevtype, develem->major, develem->minor); // static node is block to mknod
			if (slen == buflen)
				continue;	// buffer over -> drop data
		}

		bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", buf);
	}

	// gpio
	dl_list_for_each(gpioelem, &devc->static_device.static_gpiolist, container_static_gpio_elem_t, list) {
		//device bind mount
		buflen = sizeof(buf) - 1;
		memset(buf,0,sizeof(buf));

		if (gpioelem->from == NULL || gpioelem->to == NULL || develem->is_valid == 0) {
			// gpio is mandatry device
			result = -2;
			goto err_ret;
		}

		slen = snprintf(buf, buflen, "%s %s none bind", gpioelem->from, gpioelem->to); 
		if (slen == buflen)
			continue;	// buffer over -> drop data

		buflen = buflen - slen;
		if (devgpio_direction_isvalid(gpioelem->portdirection) == 1) {
			// in = read only = not need bind mount
			// out, low, high = need to rw bind mount to set gpio value
			// notset = shall set direction in guest = rw mount
			if (gpioelem->portdirection != DEVGPIO_DIRECTION_IN) {
				(void)strncat(buf, ",rw", buflen);
				slen = sizeof(",rw");

				bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
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
		buflen = sizeof(buf) - 1;
		buf[0] = '\0';

		if (iioelem->sysfrom == NULL || iioelem->systo == NULL ) {
			// iio(sysfs) parameter was broaken
			result = -2;
			goto err_ret;
		}

		if (iioelem->is_sys_valid == 0) {
			if(iioelem->optional == 1) {
				// If device is optional, no device case is skip
				#ifdef _PRINTF_DEBUG_
				fprintf(stderr,"lxcutil: lxcutil_set_config_base skip  %s\n", iioelem->sysfrom);
				#endif
				continue;
			} else {
				// Not optional, error.
				#ifdef _PRINTF_DEBUG_
				fprintf(stderr,"lxcutil: lxcutil_set_config_base not valid sys %s - error\n", iioelem->sysfrom);
				#endif
				result = -2;
				goto err_ret;
			}
		}

		slen = snprintf(buf, buflen, "%s %s none bind,rw", iioelem->sysfrom, iioelem->systo); 
		if (slen == buflen)
			continue;	// buffer over -> drop data

		bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
		if (bret == false) {
			result = -1;
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
			#endif
			goto err_ret;
		}
		// sysfs part end.

		if (iioelem->devfrom != NULL && iioelem->devto != NULL && iioelem->devnode != NULL) {
			// dev node part required.
			buflen = sizeof(buf) - 1;
			buf[0] = '\0';

			if (iioelem->is_dev_valid == 1) {
				slen = snprintf(buf, buflen, "%s %s none bind,rw", iioelem->devfrom, iioelem->devto); 
				if (slen == buflen)
					continue;	// buffer over -> drop data

				buflen = buflen - slen;
				if (develem->optional == 1) {
					(void)strncat(buf, ",optional", buflen);
					slen = sizeof(",optional");
				} else
					slen = 0;

				buflen = buflen - slen;
				(void)strncat(buf, ",create=file", buflen);

				bret = plxc->set_config_item(plxc, "lxc.mount.entry", buf);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
					#endif
					goto err_ret;
				}

				// device allow
				buflen = sizeof(buf) - 1;
				buf[0] = '\0';

				slen = snprintf(buf, buflen, "c %d:%d rw", iioelem->major, iioelem->minor); // static node is block to mknod
				if (slen == buflen)
					continue;	// buffer over -> drop data

				bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", buf);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"lxcutil: lxcutil_set_config_base set config %s = %s fail.\n", "lxc.mount.entry", buf);
					#endif
					goto err_ret;
				}
			} else {
				if(iioelem->optional == 0) {
					// Not optional, error.
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"lxcutil: lxcutil_set_config_base not valid dev %s - error\n", iioelem->devfrom);
					#endif
					result = -2;
					goto err_ret;
				}
				#ifdef _PRINTF_DEBUG_
				fprintf(stderr,"lxcutil: lxcutil_set_config_base skip  %s\n", iioelem->devfrom);
				#endif
			}
		} 
	}

	return 0;

err_ret:
	
	return result;
}
/**
 * Read json string with memory alocation
 *
 * @param [in]	file		Full file path for json file
 * @return int
 * @retval -1 lxc runtime error.
 * @retval -2 mandatory setting error.
 * @retval -3 TODO.
 */
static int lxcutil_set_config_static_netif(struct lxc_container *plxc, container_netifconfig_t *netc)
{
	int result = -1;
	bool bret = false;
	char buf[1024];
	container_static_netif_elem_t *netelem = NULL;
	int num = 0;

	memset(buf,0,sizeof(buf));

	// static net if
	dl_list_for_each(netelem, &netc->static_netif.static_netiflist, container_static_netif_elem_t, list) {
		buf[0] = '\0';

		if (netelem->type == STATICNETIF_VETH) {
			netif_elem_veth_t *veth = (netif_elem_veth_t*)netelem->setting;

			(void)snprintf(buf, sizeof(buf), "lxc.net.%d.type", num);
			bret = plxc->set_config_item(plxc, buf, "veth");
			if (bret == false) {
				result = -1;
				#ifdef _PRINTF_DEBUG_
				fprintf(stderr,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, "veth");
				#endif
				goto err_ret;
			}

			if (veth->name != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.name", num);
				bret = plxc->set_config_item(plxc, buf, veth->name);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->name);
					#endif
					goto err_ret;
				}
			}

			if (veth->link != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.link", num);
				bret = plxc->set_config_item(plxc, buf, veth->link);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->link);
					#endif
					goto err_ret;
				}
			}

			if (veth->flags != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.flags", num);
				bret = plxc->set_config_item(plxc, buf, veth->flags);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->flags);
					#endif
					goto err_ret;
				}
			}

			if (veth->hwaddr != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.hwaddr", num);
				bret = plxc->set_config_item(plxc, buf, veth->hwaddr);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->hwaddr);
					#endif
					goto err_ret;
				}
			}

			if (veth->mode != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.veth.mode", num);
				bret = plxc->set_config_item(plxc, buf, veth->mode);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->mode);
					#endif
					goto err_ret;
				}
			}

			if (veth->address != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.ipv4.address", num);
				bret = plxc->set_config_item(plxc, buf, veth->address);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->address);
					#endif
					goto err_ret;
				}
			}

			if (veth->gateway != NULL) {
				(void)snprintf(buf, sizeof(buf), "lxc.net.%d.ipv4.gateway", num);
				bret = plxc->set_config_item(plxc, buf, veth->gateway);
				if (bret == false) {
					result = -1;
					#ifdef _PRINTF_DEBUG_
					fprintf(stderr,"lxcutil: lxcutil_set_config_static_netif set config %s = %s fail.\n", buf, veth->gateway);
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
 * Read json string with memory alocation
 *
 * @param [in]	file		Full file path for json file
 * @return int
 * @retval -1 Json file error.
 * @retval -2 Json file perse error. 
 * @retval -3 Memory allocation error. 
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
		fprintf(stderr,"lxcutil: lxcutil_create_instance container %s create fail.\n",cc->name);
		#endif
		goto err_ret;
	}

	plxc->clear_config(plxc);

	bret = plxc->set_config_item(plxc, "lxc.uts.name", cc->name);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"lxcutil: lxcutil_create_instance set config %s = %s fail.\n", "lxc.uts.name", cc->name);
		#endif
		goto err_ret;
	}

	ret = lxcutil_set_config_base(plxc, &cc->baseconfig);
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

	#ifdef _PRINTF_DEBUG_
	{
		char buf[1024];

		snprintf(buf, sizeof(buf)-1, "/tmp/dbgcfg-%s.txt", cc->name);
		bret = plxc->save_config(cc->runtime_stat.lxc, buf);
		if (bret == false)
			fprintf(stderr,"lxcutil: save_config fail.\n");
		
	}
	#endif

	return 0;

err_ret:
	if (plxc != NULL)
		(void)lxc_container_put(plxc);

	return result;
}
/**
 * runtime network interface assign into guest
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0 success
 * @retval -1 critical error.
 */
int lxcutil_container_shutdown(container_config_t *cc)
{
	bool bret = false;
	int result = -1;

	if (cc->runtime_stat.lxc != NULL) {
		bret = cc->runtime_stat.lxc->shutdown(cc->runtime_stat.lxc, 0);	//non block shutdown
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "lxcutil_container_shutdown: shutdown request to guest %s\n", cc->name );
		#endif
	}

	if (bret == true)
		result = 0;

	return result;
}
/**
 * runtime network interface assign into guest
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0 success
 * @retval -1 critical error.
 */
int lxcutil_container_fourcekill(container_config_t *cc)
{
	pid_t pid = -1;

	if (cc->runtime_stat.lxc != NULL) {
		pid = cc->runtime_stat.lxc->init_pid(cc->runtime_stat.lxc);
		if (pid > 0) {
			(void) kill(pid, SIGKILL);
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr, "lxcutil_container_fourcekill: kill signal send to guest %s\n", cc->name );
			#endif
		}
	}

	return 0;
}
/**
 * release lxc instance
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0 success
 * @retval -1 critical error.
 */
int lxcutil_release_instance(container_config_t *cc)
{

	if (cc->runtime_stat.lxc != NULL)
		(void)lxc_container_put(cc->runtime_stat.lxc);

	cc->runtime_stat.lxc = NULL;

	return 0;
}

/**
 * device type chkeck sub function for lxcutil_dynamic_device_add_to_guest
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0 success
 * @retval -1 critical error.
 */
static int lxcutil_device_type_get(const char *subsystem)
{
	if (strcmp(subsystem, "block") == 0)
		return DEVNODE_TYPE_BLK;

	return DEVNODE_TYPE_CHR;
}
/**
 * device type chkeck sub function for lxcutil_dynamic_device_add_to_guest
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0 success
 * @retval -1 critical error.
 */
static int lxcutil_add_remove_guest_node_child(pid_t target_pid, const char *path, int is_add, mode_t devmode, dev_t devnum)
{
	int ret = -1;
	char buf[PATH_MAX];

	memset(buf, 0 , sizeof(buf));

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
	if (is_add != 1)
		return 0;

	strncpy(buf, path, sizeof(buf)-1);

	ret = mkdir_p(buf, 0755);
	if (ret < 0)
		return -1;

	/* create the device node */
	ret = mknod(path, devmode, devnum);
	if (ret < 0 && errno != EEXIST)
		return -1;

	return 0;
}
/**
 * device type chkeck sub function for lxcutil_dynamic_device_add_to_guest
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0 success
 * @retval -1 critical error.
 */
static int lxcutil_add_remove_guest_node(pid_t target_pid, const char *path, int is_add, dev_t devnum)
{
	int ret = -1;
	pid_t child_pid = -1;
	struct stat sb = {0};
	mode_t devmode = 0;;

	if (is_add == 1) {
		ret = stat(path, &sb);
		if (ret < 0)
			return -1;

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
	if (ret < 0)
		return -2;

	return 0;
}
/**
 * runtime device assign into guest
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0 success
 * @retval -1 critical error.
 * @retval -2 device node creatrion error.
 */
int lxcutil_dynamic_device_add_to_guest(container_config_t *cc, dynamic_device_elem_data_t *dded, int mode)
{
	bool bret = false;
	int ret = -1;
	int result = -1;
	pid_t target_pid = 0;
	int devtype = 0;
	char buf[1024];

	if (cc->runtime_stat.lxc == NULL) {
		result = -1;
		goto err_ret;
	}

	devtype =  lxcutil_device_type_get(dded->subsystem);
	if (devtype == DEVNODE_TYPE_BLK) {
		ret = snprintf(buf, sizeof(buf), "b %d:%d rwm", major(dded->devnum), minor(dded->devnum));
	} else {
		ret = snprintf(buf, sizeof(buf), "c %d:%d rwm", major(dded->devnum), minor(dded->devnum));
	} 

	if (!(ret < (sizeof(buf)-1))) {
		result = -1;
		goto err_ret;
	}

	bret = cc->runtime_stat.lxc->set_cgroup_item(cc->runtime_stat.lxc, "devices.allow", buf);
	if (bret == false) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "lxcutil_dynamic_device_add_to_guest: fail set_cgroup_item %s\n", buf);
		#endif
		result = -2;
		goto err_ret;
	}

	target_pid = cc->runtime_stat.lxc->init_pid(cc->runtime_stat.lxc);

	if ((mode & 0x1) != 0) {
		ret = lxcutil_add_remove_guest_node(target_pid, dded->devnode, 1, dded->devnum);
		if (ret < 0) {
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr, "lxcutil_dynamic_device_add_to_guest: fail lxcutil_add_remove_guest_node (%d) %s\n", ret, dded->devnode);
			#endif
			; // Continue to processing
		}
	}

	if ((mode & 0x2) != 0) {
		// uevent injection
		ret = uevent_injection_to_pid(target_pid, dded, "add");
		if (ret < 0) {
			result = -1;
			goto err_ret;
		}
	}

	return 0;
	
err_ret:

	return result;
}
/**
 * runtime device remove from guest
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0 success
 * @retval -1 critical error.
 */
int lxcutil_dynamic_device_remove_from_guest(container_config_t *cc, dynamic_device_elem_data_t *dded, int mode)
{
	bool bret = false;
	int ret = -1;
	int result = -1;
	pid_t target_pid = 0;
	int devtype = 0;
	char buf[1024];

	if (cc->runtime_stat.lxc == NULL) {
		result = -1;
		goto err_ret;
	}

	devtype =  lxcutil_device_type_get(dded->subsystem);
	if (devtype == DEVNODE_TYPE_BLK) {
		ret = snprintf(buf, sizeof(buf), "b %d:%d rwm", major(dded->devnum), minor(dded->devnum));
	} else {
		ret = snprintf(buf, sizeof(buf), "c %d:%d rwm", major(dded->devnum), minor(dded->devnum));
	} 

	if (!(ret < (sizeof(buf)-1))) {
		result = -1;
		goto err_ret;
	}

	bret = cc->runtime_stat.lxc->set_cgroup_item(cc->runtime_stat.lxc, "devices.deny", buf);
	if (bret == false) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "lxcutil_dynamic_device_remove_from_guest: fail set_cgroup_item %s\n", buf);
		#endif
		result = -2;
		goto err_ret;
	}

	target_pid = cc->runtime_stat.lxc->init_pid(cc->runtime_stat.lxc);

	if ((mode & 0x1) != 0) {
		ret = lxcutil_add_remove_guest_node(target_pid, dded->devnode, 0, dded->devnum);
		if (ret < 0) {
			#ifdef _PRINTF_DEBUG_
			fprintf(stderr, "lxcutil_dynamic_device_remove_from_guest: fail lxcutil_add_remove_guest_node(%d) %s\n", ret, dded->devnode);
			#endif
			; // Continue to processing
		}
	}

	if ((mode & 0x2) != 0) {
		// uevent injection
		ret = uevent_injection_to_pid(target_pid, dded, "remove");
		if (ret < 0) {
			result = -1;
			goto err_ret;
		}
	}

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr, "lxcutil_dynamic_device_remove_from_guest: dynamic devide %s remove from %s\n", dded->devpath, cc->name );
	#endif

	return 0;
	
err_ret:

	return result;
}
/**
 * runtime network interface assign into guest
 *
 * @param [in]	cc 	container_config_t
 * @return int
 * @retval 0 success
 * @retval -1 critical error.
 */
int lxcutil_dynamic_networkif_add_to_guest(container_config_t *cc, container_dynamic_netif_elem_t *cdne)
{
	bool bret = false;
	int result = -1;

	if (cc->runtime_stat.lxc != NULL) {
		bret = cc->runtime_stat.lxc->attach_interface(cc->runtime_stat.lxc, cdne->ifname, cdne->ifname);
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "lxcutil_dynamic_networkif_add_to_guest: network interface %s assign to %s\n", cdne->ifname, cc->name );
		#endif
	}

	if (bret == true)
		result = 0;

	return result;
}
