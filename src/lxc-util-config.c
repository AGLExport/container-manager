/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	lxc-util.c
 * @brief	LXC control interface for container manager use.
 */

#include "lxc-util.h"
#include "cm-utils.h"
#include "cgroup-utils.h"

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
		ret = snprintf(buf,sizeof(buf),"cgroup:mixed proc:mixed sys:mixed shmounts:%s", bc->extended.shmounts);
	} else {
		// The shmount option was not enabled. Set default options.
		buf[0] = '\0';
		ret = snprintf(buf,sizeof(buf),"cgroup:mixed proc:mixed sys:mixed");
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
 * Create the per container cgroup setting to avoid overwrite from guest in cgroup v1 environment.
 * It must be same interface for lxcutil_create_per_guest_cgroup.
 *
 * @param [in]	plxc	The lxc container instance to set config.
 * @param [in]	rsc		Pointer to container_resourceconfig_t.
 * @param [in]	name	String for guest name.
 * @return int
 * @retval 0	Success to set lxc config.
 * @retval -1	Got lxc error.
 * @retval -2	A bytes of config string is larger than buffer size. Critical case only.
 */
static int lxcutil_create_per_guest_cgroup_v1(struct lxc_container *plxc, container_resourceconfig_t *rsc, const char *name)
{
	int result = -1;
	bool bret = false;
	char buf[1024];
	char *tmp_str = NULL;
	ssize_t slen = 0, buflen = 0;
	int64_t ms_time = 0;

	(void) memset(buf,0,sizeof(buf));

	// When guest was crash, cgroup node will not delete.
	// To avoid name conflict, add monotonic ms time to directory name.
	ms_time = get_current_time_ms();

	//create per container cgroup
	//lxc.cgroup.dir.container
	buflen = (ssize_t)sizeof(buf) - 1;
	slen = (ssize_t)snprintf(buf, buflen, "%s-container-%lx", name, ms_time);
	if (slen >= buflen) {
		result = -2;
		goto err_ret;
	}
	bret = plxc->set_config_item(plxc, "lxc.cgroup.dir.container", buf);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_resource set config %s = %s fail.\n", "lxc.cgroup.dir.container", buf);
		#endif
		goto err_ret;
	}
	tmp_str = strdup(buf);
	if (tmp_str == NULL) {
		result = -2;
		goto err_ret;
	}
	rsc->cgroup_path_container = tmp_str;

	//lxc.cgroup.dir.container
	slen = (ssize_t)snprintf(buf, buflen, "%s-monitor-%lx", name, ms_time);
	if (slen >= buflen) {
		result = -2;
		goto err_ret;
	}
	bret = plxc->set_config_item(plxc, "lxc.cgroup.dir.monitor", buf);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_resource set config %s = %s fail.\n", "lxc.cgroup.dir.monitor", buf);
		#endif
		goto err_ret;
	}
	tmp_str = strdup(buf);
	if (tmp_str == NULL) {
		result = -2;
		goto err_ret;
	}
	rsc->cgroup_path_monitor = tmp_str;

	//lxc.cgroup.dir.container.inner
	slen = (ssize_t)snprintf(buf, buflen, "%s-ns", name);
	if (slen >= buflen) {
		result = -2;
		goto err_ret;
	}
	bret = plxc->set_config_item(plxc, "lxc.cgroup.dir.container.inner", buf);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_resource set config %s = %s fail.\n", "lxc.cgroup.dir.container.inner", buf);
		#endif
		goto err_ret;
	}
	tmp_str = strdup(buf);
	if (tmp_str == NULL) {
		result = -2;
		goto err_ret;
	}
	rsc->cgroup_subpath_container_inner = tmp_str;

	//lxc.cgroup.relative
	bret = plxc->set_config_item(plxc, "lxc.cgroup.relative", "0");
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_resource set config %s = %s fail.\n", "lxc.cgroup.relative", "1");
		#endif
		goto err_ret;
	}

	rsc->enable_cgroup_inner_outer_mode = 1;

	return 0;

err_ret:
	(void) free(rsc->cgroup_subpath_container_inner);
	rsc->cgroup_subpath_container_inner = NULL;

	(void) free(rsc->cgroup_path_monitor);
	rsc->cgroup_path_monitor = NULL;

	(void) free(rsc->cgroup_path_container);
	rsc->cgroup_path_container = NULL;

	rsc->enable_cgroup_inner_outer_mode = 0;

	return result;
}
/**
 * Create the per container cgroup setting to avoid overwrite from guest in cgroup v2 environment.
 * It must be same interface for lxcutil_create_per_guest_cgroup.
 *
 * @param [in]	plxc	The lxc container instance to set config.
 * @param [in]	rsc		Pointer to container_resourceconfig_t.
 * @param [in]	name	String for guest name.
 * @return int
 * @retval 0	Success to set lxc config.
 * @retval -1	Got lxc error.
 * @retval -2	A bytes of config string is larger than buffer size. Critical case only.
 */
static int lxcutil_create_per_guest_cgroup_v2(struct lxc_container *plxc, container_resourceconfig_t *rsc, const char *name)
{
	// This feature is not support v2 environment.
	rsc->cgroup_subpath_container_inner = NULL;
	rsc->cgroup_path_monitor = NULL;
	rsc->cgroup_path_container = NULL;
	rsc->enable_cgroup_inner_outer_mode = 0;
	return 0;
}
/**
 * Create the per container cgroup setting to avoid overwrite from guest.
 *
 * @param [in]	plxc	The lxc container instance to set config.
 * @param [in]	rsc		Pointer to container_resourceconfig_t.
 * @param [in]	name	String for guest name.
 * @return int
 * @retval 0	Success to set lxc config.
 * @retval -1	Got lxc error.
 * @retval -2	A bytes of config string is larger than buffer size. Critical case only.
 */
static int lxcutil_create_per_guest_cgroup(struct lxc_container *plxc, container_resourceconfig_t *rsc, const char *name)
{
	int result = -1;
	int ret = -1;

	ret = cgroup_util_get_cgroup_version();
	if (ret == 2) {
		result = lxcutil_create_per_guest_cgroup_v2(plxc, rsc, name);
	} else if (ret == 1) {
		result = lxcutil_create_per_guest_cgroup_v1(plxc, rsc, name);
	} else {
		result = -1;
	}

	return result;
}
/**
 * Release runtime data for the per container cgroup setting.
 *
 * @param [in]	rsc		Pointer to container_resourceconfig_t.
 * @return int
 * @retval 0	Success to release runtime data.
 */
static int lxcutil_release_per_guest_cgroup_runtime_data(container_resourceconfig_t *rsc)
{
	(void) free(rsc->cgroup_subpath_container_inner);
	rsc->cgroup_subpath_container_inner = NULL;

	(void) free(rsc->cgroup_path_monitor);
	rsc->cgroup_path_monitor = NULL;

	(void) free(rsc->cgroup_path_container);
	rsc->cgroup_path_container = NULL;

	rsc->enable_cgroup_inner_outer_mode = 0;

	return 0;
}
/**
 * Create lxc config for resource setting.
 *
 * @param [in]	plxc	The lxc container instance to set config.
 * @param [in]	node	Name of resource group node.
 * @param [in]	object	Object name.
 * @param [in]	value	Value for object.
 * @return int
 * @retval 0	Success to set lxc config from rsc.
 * @retval -1	Got lxc error.
 * @retval -2	A bytes of config string is larger than buffer size. Critical case only.
 */
static int lxcutil_set_config_resource_node(struct lxc_container *plxc, const char *node, const char *object, const char *value)
{
	int result = 0;
	bool bret = false;
	char buf[1024];
	ssize_t slen = 0, buflen = 0;

	buflen = (ssize_t)sizeof(buf) - 1;
	buf[0] = '\0';

	slen = (ssize_t)snprintf(buf, buflen, "lxc.%s.%s", node, object);
	if (slen >= buflen) {
		// buffer over -> drop data
		result = -2;
		goto do_return;
	}

	bret = plxc->set_config_item(plxc, buf, value);
	if (bret == false) {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"lxcutil: lxcutil_set_config_resource_node set config %s = %s fail.\n", buf, value);
		#endif
	}

do_return:
	return result;
}
/**
 * Create lxc config from container config resourceconfig sub part.
 *
 * @param [in]	plxc	The lxc container instance to set config.
 * @param [in]	rsc		Pointer to container_resourceconfig_t.
 * @param [in]	name	String for guest name.
 * @return int
 * @retval 0	Success to set lxc config from rsc.
 * @retval -1	Got lxc error.
 * @retval -2	A bytes of config string is larger than buffer size. Critical case only.
 */
static int lxcutil_set_config_resource(struct lxc_container *plxc, container_resourceconfig_t *rsc, const char *name)
{
	int ret = -1, result = 0;
	int cgroup_ver = -1;
	container_resource_elem_t *melem = NULL;
	#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
	int once_error = 0;
	#endif

	ret = lxcutil_create_per_guest_cgroup(plxc, rsc, name);
	if (ret < 0) {
		if (ret == -1) {
			result = -1;
		} else {
			result = -2;
		}
		goto do_return;
	}

	cgroup_ver = cgroup_util_get_cgroup_version();

	dl_list_for_each(melem, &rsc->resource.resourcelist, container_resource_elem_t, list) {
		if (melem->type == RESOURCE_TYPE_CGROUP_V1) {
			if ((melem->object == NULL) || (melem->value == NULL)) {
				continue;	//drop data
			}
			if (cgroup_ver != 1) {
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				if (once_error == 0) {
					(void) fprintf(stderr,"[CM CRITICAL INFO] Container %s has cgroup v1 setting. It was dropped.\n", name);
				}
				once_error++;
				#endif
				continue;	//drop data
			}

			ret = lxcutil_set_config_resource_node(plxc, "cgroup", melem->object, melem->value);
			if (ret < 0) {
				result = -1;
				goto do_return;
			}
		} else if (melem->type == RESOURCE_TYPE_CGROUP_V2) {
			if (melem->object == NULL || melem->value == NULL) {
				continue;	//drop data
			}
			if (cgroup_ver != 2) {
				#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
				if (once_error == 0) {
					(void) fprintf(stderr,"[CM CRITICAL INFO] Container %s has cgroup v2 setting. It was dropped.\n", name);
				}
				once_error++;
				#endif
				continue;	//drop data
			}

			ret = lxcutil_set_config_resource_node(plxc, "cgroup2", melem->object, melem->value);
			if (ret < 0) {
				result = -1;
				goto do_return;
			}
		} else if (melem->type == RESOURCE_TYPE_PRLIMIT) {
			if (melem->object == NULL || melem->value == NULL) {
				continue;	//drop data
			}

			ret = lxcutil_set_config_resource_node(plxc, "prlimit", melem->object, melem->value);
			if (ret < 0) {
				result = -1;
				goto do_return;
			}
		} else if (melem->type == RESOURCE_TYPE_SYSCTL) {
			if (melem->object == NULL || melem->value == NULL) {
				continue;	//drop data
			}

			ret = lxcutil_set_config_resource_node(plxc, "sysctl", melem->object, melem->value);
			if (ret < 0) {
				result = -1;
				goto do_return;
			}
		} else {
			; //nop
		}
	}

do_return:
	return result;
}
/**
 * Release runtime data of resourceconfig sub part.
 *
 * @param [in]	rsc		Pointer to container_resourceconfig_t.
 * @return int
 * @retval 0	Success to release runtime data for the resourceconfig.
 */
static int lxcutil_release_config_runtime_data_resource(container_resourceconfig_t *rsc)
{
	(void) lxcutil_release_per_guest_cgroup_runtime_data(rsc);

	return 0;
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
 * Set lxc config from container config deviceconfig sub part for set default.
 *
 * @param [in]	plxc		The lxc container instance to set config.
 * @param [in]	is_allow	If this parameter set true, it set allow.  If this parameter set true, it set deny.
 * @param [in]	config_str	Device setting string.
 * @return bool
 * @retval true	Success to set lxc config.
 * @retval false Fail to set lxc config.
 */
static bool lxcutil_set_cgroup_device(struct lxc_container *plxc, bool is_allow, const char *config_str)
{
	int ret = -1;
	bool bret = false;

	ret = cgroup_util_get_cgroup_version();
	if (ret == 1) {
		// cgroup v1
		if (is_allow == true) {
			bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.allow", config_str);
		} else {
			bret = plxc->set_config_item(plxc, "lxc.cgroup.devices.deny", config_str);
		}
	} else if (ret == 2) {
		// cgroup v2
		if (is_allow == true) {
			bret = plxc->set_config_item(plxc, "lxc.cgroup2.devices.allow", config_str);
		} else {
			bret = plxc->set_config_item(plxc, "lxc.cgroup2.devices.deny", config_str);
		}
	} else {
		bret = false;
	}

	return bret;
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
static const char *g_default_allow_devices[] = {
	"c 1:3 rwm",	// /dev/null
	"c 1:5 rwm",	// /dev/zero
	"c 1:7 rwm",	// /dev/full
	"c 5:0 rwm",	// /dev/tty
	"c 5:2 rwm",	// /dev/ptmx
	"c 1:8 rwm",	// /dev/random
	"c 1:9 rwm",	// /dev/urandom
	"c 136:* rwm",	// /dev/pts/x
	NULL,
};
static int lxcutil_set_config_static_device_default(struct lxc_container *plxc)
{
	int result = 0;
	bool bret = false;

	// Set all devices are deny
	bret = lxcutil_set_cgroup_device(plxc, false, "a");
	if (bret == false) {
		result = -1;
		goto err_ret;
	}

	// Set default allow devices
	for (int i=0; ; i++) {
		const char *config_str = g_default_allow_devices[i];
		if (config_str == NULL) {
			break;
		}

		bret = lxcutil_set_cgroup_device(plxc, true, config_str);
		if (bret == false) {
			result = -1;
			goto err_ret;
		}
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
		} else if (develem->type == DEVICE_TYPE_DEVDIR) {
			(void) strncpy(&buf[slen], ",create=dir", buflen);
			slen = slen + (ssize_t)sizeof(",create=dir") - 1;
		} else {
			;//nop
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

		bret = lxcutil_set_cgroup_device(plxc, true, buf);
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

				bret = lxcutil_set_cgroup_device(plxc, true, buf);
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

	ret = lxcutil_set_config_resource(plxc, &cc->resourceconfig, cc->name);
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
	plxc = NULL;
	cc->runtime_stat.pid = -1;

	#ifdef PRINTF_DEBUG_CONFIG_OUT
	{
		char buf[1024];

		(void) snprintf(buf, sizeof(buf)-1u, "/tmp/dbgcfg-%s.txt", cc->name);
		bret = cc->runtime_stat.lxc->save_config(cc->runtime_stat.lxc, buf);
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

	(void) lxcutil_release_instance(cc);

	return result;
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
		(void) lxc_container_put(cc->runtime_stat.lxc);
	}

	(void) lxcutil_release_config_runtime_data_resource(&cc->resourceconfig);

	cc->runtime_stat.lxc = NULL;
	cc->runtime_stat.pid = -1;

	return 0;
}