/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	lxc-util.c
 * @brief	LXC control interface for container manager use.
 */

#include "lxc-util.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <lxc/lxccontainer.h>

#include "cm-utils.h"
#include "uevent_injection.h"

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
	if (!((size_t)ret < sizeof(buf)-1u)) {
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

	(void) strncpy(buf, path, sizeof(buf)-1u);

	ret = mkdir_p(buf, 0755);
	if (ret < 0) {
		return -1;
	}

	/* create the device node */
	ret = mknod(path, devmode, devnum);
	if ((ret < 0) && (errno != EEXIST)) {
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
		ret = lstat(path, &sb);
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
		if (ret < 0) {
			_exit(EXIT_FAILURE);
		}

		_exit(EXIT_SUCCESS);
	}

	ret = wait_child_pid(child_pid);
	if (ret < 0) {
		return -2;
	}

	return 0;
}
/**
 * The function of cgroup device group operation.
 * Device allow/deny setting by cgroup.
 *
 * @param [in]	cc		Pointer to container_config_t of target container.
 * @param [in]	is_add	This operation is add or remove? remove: ==0, add: !=0.
 * @param [in]	value	The value for device allow/deny setting.
 * @return int
 * @retval 0	Success to operations.
 * @retval -1	Critical error.
 */
static const char *cgroup_fs_devices_base_path = "/sys/fs/cgroup/devices";
int lxcutil_cgroup_device_operation(container_config_t *cc, int is_add, const char *value)
{
	int ret = -1;
	int result = -1;
	size_t value_length = 0;
	char *operation_node = NULL;
	char buf[PATH_MAX];

	// Device allow/deny setting using cgroup.
	value_length = strlen(value);
	if (value_length == 0) {
		// Can't operate this value.
		result = -1;
		goto err_ret;
	}

	if (is_add == 0) {
		operation_node = "devices.deny";
	} else {
		operation_node = "devices.allow";
	}

	// Path for intermediate group
	ret = snprintf(buf, sizeof(buf), "%s/%s/%s", cgroup_fs_devices_base_path
					, cc->resourceconfig.cgroup_path_container, operation_node);
	if (((size_t)ret) >= sizeof(buf)) {
		// Too long path. Error.
		result = -1;
		goto err_ret;
	}
	// Write value to ptah.
	(void) once_write(buf, value, value_length);

	// Path for guest group
	ret = snprintf(buf, sizeof(buf), "%s/%s/%s/%s", cgroup_fs_devices_base_path
					, cc->resourceconfig.cgroup_path_container, cc->resourceconfig.cgroup_subpath_container_inner
					, operation_node);
	if (((size_t)ret) >= sizeof(buf)) {
		// Too long path. Error.
		result = -1;
		goto err_ret;
	}
	// Write value to ptah.
	(void) once_write(buf, value, value_length);

	// Option for systemd.  Add value to system.slice.
	ret = snprintf(buf, sizeof(buf), "%s/%s/%s/%s/%s", cgroup_fs_devices_base_path
					, cc->resourceconfig.cgroup_path_container, cc->resourceconfig.cgroup_subpath_container_inner
					, "system.slice", operation_node);
	if (((size_t)ret) >= sizeof(buf)) {
		// Too long path. Error.
		result = -1;
		goto err_ret;
	}
	// Write value to ptah.
	(void) once_write(buf, value, value_length);

	return 0;

err_ret:

	return result;
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

	if ((lddr->dev_major < 0) || (lddr->dev_minor < 0)) {
		return 0;	// No need to allow/deny device by cgroup
	}

	// Device allow/deny setting by cgroup.
	if (lddr->is_allow_device == 1) {
		const char *permission = NULL;
		const char perm_default[] = "rw";
		char buf[1024];

		// device allow/deny only to operate add/remove operation.
		if ((lddr->operation == DCD_UEVENT_ACTION_ADD) || (lddr->operation == DCD_UEVENT_ACTION_REMOVE)) {
			permission = lddr->permission;
			if (permission == NULL) {
				permission = perm_default;
			}

			if (lddr->devtype == DEVNODE_TYPE_BLK) {
				ret = snprintf(buf, sizeof(buf), "b %d:%d %s", lddr->dev_major, lddr->dev_minor, permission);
			} else {
				ret = snprintf(buf, sizeof(buf), "c %d:%d %s", lddr->dev_major, lddr->dev_minor, permission);
			}

			if (!((size_t)ret < (sizeof(buf)-1u))) {
				result = -1;
				goto err_ret;
			}

			if (lddr->operation == DCD_UEVENT_ACTION_ADD) {
				ret = lxcutil_cgroup_device_operation(cc, 1, buf);
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout, "lxcutil_cgroup_device_operation: %s = %s\n", "devices.allow", buf);
				#endif
			} else if (lddr->operation == DCD_UEVENT_ACTION_REMOVE) {
				// In case of block device, guest must be unmount device. In this case need to access capability.
				if (lddr->devtype != DEVNODE_TYPE_BLK) {
					ret = lxcutil_cgroup_device_operation(cc, 0, buf);
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout, "lxcutil_cgroup_device_operation: %s = %s\n", "devices.deny", buf);
					#endif
				} else {
					ret = 0;
				}
			} else {
				// May not use this path
				ret = -1;
			}

			if (ret < 0) {
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout, "lxcutil_dynamic_device_operation: fail set_cgroup_item %s\n", buf);
				#endif
				result = -2;
				goto err_ret;
			}
		}
	}

	// Device node creation and remove.
	if (lddr->is_create_node == 1) {
		dev_t devnum = 0;
		ret = -1;
		pid_t target_pid = 0;

		// device node only to operate add/remove operation.
		if ((lddr->operation == DCD_UEVENT_ACTION_ADD) || (lddr->operation == DCD_UEVENT_ACTION_REMOVE)) {
			target_pid = lxcutil_get_init_pid(cc);

			devnum = makedev(lddr->dev_major, lddr->dev_minor);
			if (lddr->operation == DCD_UEVENT_ACTION_ADD) {
				ret = lxcutil_add_remove_guest_node(target_pid, lddr->devnode, 1, devnum);
			} else if (lddr->operation == DCD_UEVENT_ACTION_REMOVE) {
				ret = lxcutil_add_remove_guest_node(target_pid, lddr->devnode, 0, devnum);
			} else {
				// May not use this path
				bret = false;
			}
			if (ret < 0) {
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout, "lxcutil_dynamic_device_operation: fail lxcutil_add_remove_guest_node (%d) %s\n", ret, lddr->devnode);
				#endif
				result = -1;
				goto err_ret;
			}
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
/**
 * Dynamic bind mount from host to guest container.
 * This function mount host directory to guest container using lxc mount interface.
 *
 * @param [in]	cc			Pointer to container_config_t.
 * @param [in]	host_path	Path for dynamic mount source.
 * @param [in]	guest_path	Path for dynamic mount target.
 * @return int
 * @retval 0	Success to operations.
 * @retval -1	Got lxc error.
 */
int lxcutil_dynamic_mount_to_guest(container_config_t *cc, const char *host_path, const char *guest_path)
{
	int ret = -1;
	int result = -1;
	struct lxc_mount mnt;

	(void) memset(&mnt, 0, sizeof(mnt));
	mnt.version = LXC_MOUNT_API_V1;


	if ((cc->runtime_stat.lxc != NULL) && (host_path != NULL) && (guest_path != NULL)) {
		/*	int (*mount)(struct lxc_container *c, const char *source,
		     const char *target, const char *filesystemtype,
		     unsigned long mountflags, const void *data,
		     struct lxc_mount *mnt);
		*/
		ret = cc->runtime_stat.lxc->mount(cc->runtime_stat.lxc, host_path, guest_path, NULL, MS_BIND, NULL, &mnt);
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout, "lxcutil_dynamic_mount_to_guest: from %s to %s ret = %d\n", host_path, guest_path, ret);
		#endif
	}

	if (ret == 0) {
		result = 0;
	}

	return result;
}
