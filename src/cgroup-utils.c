/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cgroup-utils.c
 * @brief	cgroup utility for container manager.
 */
#include "cgroup-utils.h"

#include <sys/vfs.h>
#include <linux/magic.h>
#include <stdio.h>
#include <string.h>

#include "cm-utils.h"


static int g_cgroup_version = -1;
/**
 * Test a cgroup version in own system.
 *
 * @return int
 * @retval  2 Cgroup v2.
 * @retval  1 Cgroup v1.
 * @retval  0 Undefined version.
 * @retval -1 Can not get cgroup version.
 */
int cgroup_util_get_cgroup_version(void)
{
	int ret = -1;

	if (g_cgroup_version != -1) {
		goto do_return;
	} else {
		struct statfs sfs = {0};

		ret = statfs("/sys/fs/cgroup/", &sfs);
		if (ret < 0) {
			g_cgroup_version = -1;
			goto do_return;
		}

		if (sfs.f_type == CGROUP2_SUPER_MAGIC) {
			g_cgroup_version = 2;
		} else if (sfs.f_type == TMPFS_MAGIC) {
			g_cgroup_version = 1;
		} else {
			g_cgroup_version = 0;
		}
	}

do_return:
	return 		g_cgroup_version;
}

static const char *g_cgroup_v2_subsystems[] = {
	"memory",
	"cpu",
	"cpuset",
	"pids",
	NULL,
};
static const char g_cgroup_v2_config_path[] = "/sys/fs/cgroup/cgroup.subtree_control";
/**
 * Setup a cgroup subsystem in own system.
 *
 * @return int
 * @retval  0 Success to setup.
 * @retval -1 Could not setup subsystem.
 */
int cgroup_util_cgroup_v2_setup(void)
{
	int ret = -1, result = 0;
	char buf[1024];

	(void) memset(buf,0,sizeof(buf));

	for (int i=0; ;i++) {
		ssize_t szlen = 0;
		const char* cgv2_subsys = g_cgroup_v2_subsystems[i];
		if (cgv2_subsys == NULL) {
			break;
		}

		szlen = snprintf(buf, (sizeof(buf)-1u), "+%s", cgv2_subsys);
		if (szlen >= (sizeof(buf)-1u)) {
			// Fail safe
			continue;
		}

		ret = once_write(g_cgroup_v2_config_path, buf, szlen);
		if (ret < 0) {
			result = -1;
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			(void) fprintf(stderr,"[CM CRITICAL ERROR] Current environment is not supporting %s subsystem in cgroup-v2.\n", cgv2_subsys);
			#endif
			goto do_return;
		}
	}

do_return:
	return result;
}