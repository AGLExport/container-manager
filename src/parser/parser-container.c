/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	parser.c
 * @brief	config file parser using cjson
 */

#undef _PRINTF_DEBUG_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include <cjson/cJSON.h>
#include "list.h"

#include "container.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser/parser-common.h"
#include "parser/parser-container.h"

static const char *cstr_signal_default = "SIGTERM";	/** < default signal to use guest container termination. */

/**
 * Sub function for the idmap parse.
 * Shall not call from other than cmparser_parse_base.
 *
 * @param [in]	map A container_baseconfig_idmap_t object
 * @param [in]	idmap Json object for idmap element
 * @param [in]	tc String for which id, only to use debugging log.
 * @return int
 * @retval 1 Enable idmap.
 * @retval 0 Disable idmap.
 */
static int cmparser_parse_basesub_idmap(container_baseconfig_idmap_t *map, const cJSON *idmap, const char *tc)
{
	int isenable = 1;

	if (cJSON_IsObject(idmap)) {
		cJSON *guestroot = NULL, *hostidstart = NULL, *num = NULL;

		guestroot = cJSON_GetObjectItemCaseSensitive(idmap, "guestroot");
		if (cJSON_IsNumber(guestroot)) {
			map->guest_root_id = guestroot->valueint;
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-idmaps.%s.guest_root_id = %d\n", tc, map->guest_root_id);
			#endif
		} else {
			isenable = 0;
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-idmaps.%s.guest_root_id is error\n", tc);
			#endif
		}

		hostidstart = cJSON_GetObjectItemCaseSensitive(idmap, "hostidstart");
		if (cJSON_IsNumber(hostidstart)) {
			map->host_start_id = hostidstart->valueint;
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-idmaps.%s.host_start_id = %d\n", tc, map->host_start_id);
			#endif
		} else {
			isenable = 0;
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-idmaps.%s.host_start_id is error\n", tc);
			#endif
		}

		num = cJSON_GetObjectItemCaseSensitive(idmap, "num");
		if (cJSON_IsNumber(num)) {
			map->num_of_id = num->valueint;
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-idmaps.%s.num_of_id = %d\n",tc, map->num_of_id);
			#endif
		} else {
			isenable = 0;
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-idmaps.%s.num_of_id is error\n", tc);
			#endif
		}
	}

	return isenable;
}
/**
 * Sub function for the disk mount mode parse.
 * Shall not call from other than cmparser_parse_base.
 *
 * @param [in]	str		string of fstype
 * @return int
 * @retval DISKMOUNT_TYPE_RO	str is "ro" or other
 * @retval DISKMOUNT_TYPE_RW	str is "rw"
 */
static int cmparser_parser_get_diskmountmode(const char *str)
{
	static const char read_only[] = "ro";
	static const char read_write[] = "rw";
	int ret = DISKMOUNT_TYPE_RO;

	if (strncmp(read_only, str, sizeof(read_only)) == 0)
		ret = DISKMOUNT_TYPE_RO;
	else if (strncmp(read_write, str, sizeof(read_write)) == 0)
		ret = DISKMOUNT_TYPE_RW;

	return ret;
}
/**
 * Sub function for the disk mount fail operation parse.
 * Shall not call from other than cmparser_parse_base.
 *
 * @param [in]	str		string of fstype
 * @return int
 * @retval DISKREDUNDANCY_TYPE_FAILOVER	str is "failover" or other
 * @retval DISKREDUNDANCY_TYPE_AB	str is "ab"
 */
static int cmparser_parser_get_diskmountfailop(const char *str)
{
	static const char failover[] = "failover";
	static const char ab[] = "ab";
	int ret = DISKREDUNDANCY_TYPE_FAILOVER;

	if (strncmp(failover, str, sizeof(failover)) == 0)
		ret = DISKREDUNDANCY_TYPE_FAILOVER;
	else if (strncmp(ab, str, sizeof(ab)) == 0)
		ret = DISKREDUNDANCY_TYPE_AB;

	return ret;
}
/**
 * Sub function for the rootfs config parser.
 *
 * @param [out]	bc	Pointer to pre-allocated container_baseconfig_t.
 * @param [in]	rootfs	Pointer to cJSON object of top of base section.
 * @return int
 * @retval  0 Success to parse.
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
static int cmparser_parse_base_rootfs(container_baseconfig_t *bc, const cJSON *rootfs)
{
	int result = -1;
	cJSON *path = NULL, *filesystem = NULL, *mode = NULL, *blockdev = NULL;

	path = cJSON_GetObjectItemCaseSensitive(rootfs, "path");
	if (cJSON_IsString(path) && (path->valuestring != NULL)) {
		bc->rootfs.path = strdup(path->valuestring);
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: base-path value = %s\n",bc->rootfs.path);
		#endif
	} else {
		// Mandatory value
		#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
		fprintf(stderr,"[CM CRITICAL ERROR] cmparser: base-path not set. It's mandatory value\n");
		#endif
		result = -2;
		goto err_ret;
	}

	filesystem = cJSON_GetObjectItemCaseSensitive(rootfs, "filesystem");
	if (cJSON_IsString(filesystem) && (filesystem->valuestring != NULL)) {
		bc->rootfs.filesystem = strdup(filesystem->valuestring);
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: filesystem value = %s\n",bc->rootfs.filesystem);
		#endif
	} else {
		// Mandatory value
		#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
		fprintf(stderr,"[CM CRITICAL ERROR] cmparser: filesystem not set. It's mandatory value\n");
		#endif
		result = -2;
		goto err_ret;
	}

	mode = cJSON_GetObjectItemCaseSensitive(rootfs, "mode");
	if (cJSON_IsString(mode) && (mode->valuestring != NULL)) {
		bc->rootfs.mode = cmparser_parser_get_diskmountmode(mode->valuestring);
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: mode value = %d\n",bc->rootfs.mode);
		#endif
	} else {
		// When don't have disk mount mode setting, It's use ro mount as a default.
		bc->rootfs.mode = DISKMOUNT_TYPE_RO;
	}

	blockdev = cJSON_GetObjectItemCaseSensitive(rootfs, "blockdev");
	if (cJSON_IsArray(blockdev)) {
		cJSON *dev = NULL;
		int i = 0;


		cJSON_ArrayForEach(dev, blockdev) {
			if (i < 2) {
				if (cJSON_IsString(dev) && (dev->valuestring != NULL)) {
					bc->rootfs.blockdev[i] = strdup(dev->valuestring);
					#ifdef _PRINTF_DEBUG_
					fprintf(stdout,"cmparser: base-path blockdev[%d] = %s\n",i,bc->rootfs.blockdev[i]);
					#endif
				} else {
					bc->rootfs.blockdev[i] = NULL;
					#ifdef _PRINTF_DEBUG_
					fprintf(stdout,"cmparser: base-path blockdev[%d] set default value = NULL\n", i);
					#endif
				}
			}
			i++;
		}
	}

	return 0;

err_ret:

	return result;
}
/**
 * Sub function for the extradisk config parser.
 *
 * @param [out]	bc	Pointer to pre-allocated container_baseconfig_t.
 * @param [in]	extradisk	Pointer to cJSON object of top of base section.
 * @return int
 * @retval  0 Success to parse.
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
static int cmparser_parse_base_extradisk(container_baseconfig_t *bc, const cJSON *extradisk)
{
	int result = -1;
	cJSON *disk = NULL;

	cJSON_ArrayForEach(disk, extradisk) {
		cJSON *from = NULL, *to = NULL,  *blockdev = NULL;
		cJSON *filesystem = NULL, *mode = NULL, *redundancy = NULL;
		container_baseconfig_extradisk_t *exdisk = NULL;
		int mntmode = 0, mntredundancy = 0;
		char *bdev[2], *fsstr = NULL;
		bdev[0] = NULL;
		bdev[1] = NULL;

		from = cJSON_GetObjectItemCaseSensitive(disk, "from");
		if (cJSON_IsString(from) && (from->valuestring != NULL)) {
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-extradisk from = %s\n",from->valuestring);
			#endif
			;
		} else {
			// Mandatory value, drop this entry.
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			fprintf(stderr,"[CM CRITICAL ERROR] cmparser: base-extradisk from not set. It's mandatory value. drop entry\n");
			#endif
			continue;
		}

		to = cJSON_GetObjectItemCaseSensitive(disk, "to");
		if (cJSON_IsString(to) && (to->valuestring != NULL)) {
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-extradisk to = %s\n",to->valuestring);
			#endif
			;
		} else {
			// Mandatory value, drop this entry.
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			fprintf(stderr,"[CM CRITICAL ERROR] cmparser: base-extradisk to not set. It's mandatory value. drop entry\n");
			#endif
			continue;
		}

		filesystem = cJSON_GetObjectItemCaseSensitive(disk, "filesystem");
		if (cJSON_IsString(filesystem) && (filesystem->valuestring != NULL)) {
			fsstr = filesystem->valuestring;
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-extradisk filesystem = %s\n",filesystem->valuestring);
			#endif
		} else {
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			fprintf(stderr,"[CM CRITICAL ERROR] cmparser: base-extradisk filesystem not set. It's mandatory value. drop entry\n");
			#endif
			fsstr = NULL;
		}

		mode = cJSON_GetObjectItemCaseSensitive(disk, "mode");
		if (cJSON_IsString(mode) && (mode->valuestring != NULL)) {
			mntmode = cmparser_parser_get_diskmountmode(mode->valuestring);
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-extradisk mode = %s\n",mode->valuestring);
			#endif
		} else {
			// When don't have disk mount mode setting, It's use ro mount as a default.
			mntmode = DISKMOUNT_TYPE_RO;
		}

		redundancy = cJSON_GetObjectItemCaseSensitive(disk, "redundancy");
		if (cJSON_IsString(redundancy) && (redundancy->valuestring != NULL)) {
			mntredundancy = cmparser_parser_get_diskmountfailop(redundancy->valuestring);
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-extradisk redundancy = %s\n",redundancy->valuestring);
			#endif
		} else {
			// When don't have disk mount mode setting, It's use failover as a default.
			mntredundancy = DISKREDUNDANCY_TYPE_FAILOVER;
		}

		blockdev = cJSON_GetObjectItemCaseSensitive(disk, "blockdev");
		if (cJSON_IsArray(blockdev)) {
			cJSON *dev = NULL;
			int i = 0;

			cJSON_ArrayForEach(dev, blockdev) {
				if (i < 2) {
					if (cJSON_IsString(dev) && (dev->valuestring != NULL)) {
						bdev[i] = dev->valuestring;
						#ifdef _PRINTF_DEBUG_
						fprintf(stdout,"cmparser: base-extradisk blockdev[%d] = %s\n", i, bdev[i]);
						#endif
					} else {
						bdev[i] = NULL;
						#ifdef _PRINTF_DEBUG_
						fprintf(stdout,"cmparser: base-extradisk blockdev[%d] set default value = NULL\n", i);
						#endif
					}
				}
				i++;
			}
		}

		if(bdev[0] == NULL) {
			// Mandatory value, drop this entry.
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			fprintf(stderr,"[CM CRITICAL ERROR] cmparser: base-extradisk blockdev[0] not set. It's mandatory value. drop entry\n");
			#endif
			continue;
		}

		// When no error, create add list
		exdisk = (container_baseconfig_extradisk_t*)malloc(sizeof(container_baseconfig_extradisk_t));
		if (exdisk == NULL) {
			result = -3;
			goto err_ret;
		}
		memset(exdisk, 0 , sizeof(container_baseconfig_extradisk_t));

		dl_list_init(&exdisk->list);
		exdisk->from = strdup(from->valuestring);
		exdisk->to = strdup(to->valuestring);
		if (fsstr != NULL)
			exdisk->filesystem = strdup(fsstr);
		exdisk->mode = mntmode;
		exdisk->redundancy = mntredundancy;
		exdisk->blockdev[0] = strdup(bdev[0]);
		if (bdev[1] != NULL)
			exdisk->blockdev[1] = strdup(bdev[1]);

		dl_list_add_tail(&bc->extradisk_list, &exdisk->list);
	}

	return 0;

err_ret:

	return result;
}

/**
 * parser for base section of container config.
 *
 * @param [out]	bc		Pointer to pre-allocated container_baseconfig_t.
 * @param [in]	base	Pointer to cJSON object of top of base section.
 * @return int
 * @retval  0 Success to parse.
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
static int cmparser_parse_base(container_baseconfig_t *bc, const cJSON *base)
{
	cJSON *autoboot = NULL;
	cJSON *bootpriority = NULL;
	cJSON *role = NULL;
	cJSON *rootfs = NULL;
	cJSON *extradisk = NULL;
	cJSON *lifecycle = NULL;
	cJSON *cap = NULL;
	cJSON *idmap = NULL;
	cJSON *environment = NULL;
	int result = -1;

	// Get autoboot data
	autoboot = cJSON_GetObjectItemCaseSensitive(base, "autoboot");
	if (cJSON_IsBool(autoboot)) {
		if (cJSON_IsTrue(autoboot))
			bc->autoboot = 1;
		else
			bc->autoboot = 0;

		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: base-autoboot value = %d\n",bc->autoboot);
		#endif
	} else {
		bc->autoboot = 0; // Default value is 0
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: base-autoboot set default value = 0\n");
		#endif
	}

	// Get boot priority
	bootpriority = cJSON_GetObjectItemCaseSensitive(base, "bootpriority");
	if (cJSON_IsNumber(bootpriority)) {
		bc->bootpriority = bootpriority->valueint;
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: base-bootpriority value = %d\n",bc->bootpriority);
		#endif
	} else {
		bc->bootpriority = 1000; // Default value is 100s0
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: base-autoboot set default value = 1000\n");
		#endif
	}

	// Get container role
	role = cJSON_GetObjectItemCaseSensitive(base, "role");
	if (cJSON_IsString(role) && (role->valuestring != NULL)) {
		bc->role = strdup(role->valuestring);
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: base-role value = %s\n",bc->role);
		#endif
	} else {
		// When it not set, role is default = NULL
		bc->role = strdup("default");
		#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
		fprintf(stderr,"cmparser: base-role value is default (NULL)\n");
		#endif
	}

	// Get rootfs part
	rootfs = cJSON_GetObjectItemCaseSensitive(base, "rootfs");
	if (cJSON_IsObject(rootfs)) {
		result = cmparser_parse_base_rootfs(bc, rootfs);
		if (result != 0)
			goto err_ret;
	} else {
		// Mandatory value
		#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
		fprintf(stderr,"[CM CRITICAL ERROR] cmparser: rootfs not set. It's mandatory value\n");
		#endif
		result = -2;
		goto err_ret;
	}

	// Get persistence part
	extradisk = cJSON_GetObjectItemCaseSensitive(base, "extradisk");
	if (cJSON_IsArray(extradisk)) {
		result = cmparser_parse_base_extradisk(bc, extradisk);
		if (result != 0)
			goto err_ret;
	}

	// Get lifecycle data
	lifecycle = cJSON_GetObjectItemCaseSensitive(base, "lifecycle");
	if (cJSON_IsObject(lifecycle)) {
		cJSON *halt = NULL, *reboot = NULL, *timeout = NULL;

		halt = cJSON_GetObjectItemCaseSensitive(lifecycle, "halt");
		if (cJSON_IsString(halt) && (halt->valuestring != NULL)) {
			bc->lifecycle.halt = strdup(halt->valuestring);
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-lifecycle-halt value = %s\n",bc->lifecycle.halt);
			#endif
		}

		reboot = cJSON_GetObjectItemCaseSensitive(lifecycle, "reboot");
		if (cJSON_IsString(reboot) && (reboot->valuestring != NULL)) {
			bc->lifecycle.reboot = strdup(reboot->valuestring);
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-lifecycle-reboot value = %s\n",bc->lifecycle.reboot);
			#endif
		}

		// Get boot priority
		timeout = cJSON_GetObjectItemCaseSensitive(lifecycle, "timeout");
		if (cJSON_IsNumber(timeout) && (timeout->valueint > 0)) {
			bc->lifecycle.timeout = timeout->valueint;
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-timeout value = %d\n",bc->lifecycle.timeout);
			#endif
		} else {
			bc->lifecycle.timeout = 1000; // Default value is 1000ms
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-timeout set default value = 1000\n");
			#endif
		}
	}
	if (bc->lifecycle.halt == NULL) {
		bc->lifecycle.halt = strdup(cstr_signal_default);
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: base-lifecycle-halt set default value = %s\n",bc->lifecycle.halt);
		#endif
	}
	if (bc->lifecycle.reboot == NULL) {
		bc->lifecycle.reboot = strdup(cstr_signal_default);
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: base-lifecycle-reboot set default value = %s\n",bc->lifecycle.reboot);
		#endif
	}

	// Get capability data
	cap = cJSON_GetObjectItemCaseSensitive(base, "cap");
	if (cJSON_IsObject(cap)) {
		cJSON *drop = NULL, *keep = NULL;

		drop = cJSON_GetObjectItemCaseSensitive(cap, "drop");
		if (cJSON_IsString(drop) && (drop->valuestring != NULL)) {
			bc->cap.drop = strdup(drop->valuestring);
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-cap-drop value = %s\n",bc->cap.drop);
			#endif
		}

		keep = cJSON_GetObjectItemCaseSensitive(cap, "keep");
		if (cJSON_IsString(keep) && (keep->valuestring != NULL)) {
			bc->cap.keep = strdup(keep->valuestring);
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base-cap-keep value = %s\n",bc->cap.keep);
			#endif
		}
	}

	// Get idmap data
	idmap = cJSON_GetObjectItemCaseSensitive(base, "idmap");
	if (cJSON_IsObject(idmap)) {
		cJSON *uid = NULL, *gid= NULL;
		int uidenable = 0, gidenable = 0;

		uid = cJSON_GetObjectItemCaseSensitive(idmap, "uid");
		if (cJSON_IsObject(uid)) {
			uidenable = cmparser_parse_basesub_idmap(&bc->idmaps.uid, uid, "uid");
		}

		gid = cJSON_GetObjectItemCaseSensitive(idmap, "gid");
		if (cJSON_IsObject(uid)) {
			gidenable = cmparser_parse_basesub_idmap(&bc->idmaps.gid, gid, "gid");
		}

		if (uidenable == 1 && gidenable == 1) {
			bc->idmaps.enabled = 1;
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: idmap enable\n");
			#endif
		} else {
			bc->idmaps.enabled = 0;
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: idmap disable  [uid = %d][gid = %d]\n", uidenable, gidenable);
			#endif
		}
	} else {
		bc->idmaps.enabled = 0;
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: idmap disable\n");
		#endif
	}

	// Get environment variable data
	environment = cJSON_GetObjectItemCaseSensitive(base, "environment");
	if (cJSON_IsArray(environment)) {
		cJSON *env = NULL;
		container_baseconfig_env_t *p = NULL;

		cJSON_ArrayForEach(env, environment) {
			if (cJSON_IsString(env) && (env->valuestring != NULL)) {
				p = (container_baseconfig_env_t*)malloc(sizeof(container_baseconfig_env_t));
				if (p != NULL) {

					memset(p, 0 , sizeof(container_baseconfig_env_t));
					dl_list_init(&p->list);

					p->envstring = strdup(env->valuestring);
					dl_list_add_tail(&bc->envlist, &p->list);
					#ifdef _PRINTF_DEBUG_
					fprintf(stdout,"cmparser: baseconfig.envlist add [ %s ]\n",p->envstring);
					#endif
				}
			}
		}
	}

	return 0;

err_ret:

	return result;
}
/**
 * Sub function for the resourcetype parse.
 * Shall not call from other than cmparser_parse_resource.
 *
 * @param [in]	str		string of resource control type
 * @return int
 * @retval RESOURCE_TYPE_CGROUP	str is "cgroup"
 * @retval 0 NON
 */
static int cmparser_parser_get_resourcetype(const char *str)
{
	static const char ccgroup[] = "cgroup";
	int ret = 0;

	if (strncmp(ccgroup, str, sizeof(ccgroup)) == 0)
		ret = RESOURCE_TYPE_CGROUP;

	return ret;
}

/**
 * parser for resource section of container config.
 *
 * @param [out]	rc	Pointer to pre-allocated container_resourceconfig_t.
 * @param [in]	res	Pointer to cJSON object of top of resource section.
 * @return int
 * @retval  0 Success to parse.
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
static int cmparser_parse_resource(container_resourceconfig_t *rc, const cJSON *res)
{
	int result = -1;

	// Get mount data
	if (cJSON_IsArray(res)) {
		cJSON *elem = NULL;
		cJSON *type = NULL, *object = NULL, *value = NULL;
		container_resource_elem_t *p = NULL;
		int typeval = -1;

		cJSON_ArrayForEach(elem, res) {
			if (cJSON_IsObject(elem)) {
				type = cJSON_GetObjectItemCaseSensitive(elem, "type");
				if (cJSON_IsString(type) && (type->valuestring != NULL)) {
					typeval = cmparser_parser_get_resourcetype(type->valuestring);
					if (typeval == 0)
						continue;
					#ifdef _PRINTF_DEBUG_
					fprintf(stdout,"cmparser: resource.type = %d\n",typeval);
					#endif
				} else
					continue;

				object = cJSON_GetObjectItemCaseSensitive(elem, "object");
				if (!(cJSON_IsString(object) && (object->valuestring != NULL)))
					continue;

				value = cJSON_GetObjectItemCaseSensitive(elem, "value");
				if (!(cJSON_IsString(value) && (value->valuestring != NULL)))
					continue;

				// all data available
				p = (container_resource_elem_t*)malloc(sizeof(container_resource_elem_t));
				if (p == NULL)
					goto err_ret;

				memset(p, 0 , sizeof(container_resource_elem_t));
				dl_list_init(&p->list);
				p->type = typeval;
				p->object = strdup(object->valuestring);
				p->value = strdup(value->valuestring);
				#ifdef _PRINTF_DEBUG_
				fprintf(stdout,"cmparser: resource.type = %d, from = %s, value = %s\n",
							p->type, p->object, p->value);
				#endif

				dl_list_add_tail(&rc->resource.resourcelist, &p->list);
			}
		}
	}

	return 0;

err_ret:
	{
		container_resource_elem_t *melem = NULL;
		// resource config
		while(dl_list_empty(&rc->resource.resourcelist) == 0) {
			melem = dl_list_last(&rc->resource.resourcelist, container_resource_elem_t, list);
			dl_list_del(&melem->list);
			free(melem->object);
			free(melem->value);
			free(melem);
		}
	}

	return result;
}

/**
 * Sub function for the fstype parse.
 * Shall not call from other than cmparser_parse_fs.
 *
 * @param [in]	str		string of fstype
 * @return int
 * @retval FSMOUNT_TYPE_FILESYSTEM	str is "filesystem"
 * @retval FSMOUNT_TYPE_DIRECTORY	str is "directory"
 * @retval 0 NON
 */
static int cmparser_parser_get_fstype(const char *str)
{
	static const char cfs[] = "filesystem";
	static const char cdir[] = "directory";
	static const char cdir_bc[] = "directry"; //backward compatibility - fix typo
	int ret = 0;

	if (strncmp(cfs, str, sizeof(cfs)) == 0)
		ret = FSMOUNT_TYPE_FILESYSTEM;
	else if (strncmp(cdir, str, sizeof(cdir)) == 0)
		ret = FSMOUNT_TYPE_DIRECTORY;
	else if (strncmp(cdir, str, sizeof(cdir_bc)) == 0) //backward compatibility - fix typo
		ret = FSMOUNT_TYPE_DIRECTORY;

	return ret;
}

/**
 * parser for fs section of container config.
 *
 * @param [out]	bc	Pointer to pre-allocated container_fsconfig_t.
 * @param [in]	bc	Pointer to cJSON object of top of fs section.
 * @return int
 * @retval  0 Success to parse.
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
static int cmparser_parse_fs(container_fsconfig_t *fc, const cJSON *fs)
{
	cJSON *mount = NULL;
	int result = -1;

	// Get mount data
	mount = cJSON_GetObjectItemCaseSensitive(fs, "mount");
	if (cJSON_IsArray(mount)) {
		cJSON *elem = NULL;
		cJSON *type = NULL, *from = NULL, *to = NULL;
		cJSON *fstype = NULL, *option = NULL;
		container_fsmount_elem_t *p = NULL;
		int typeval = -1;

		cJSON_ArrayForEach(elem, mount) {
			if (cJSON_IsObject(elem)) {
				type = cJSON_GetObjectItemCaseSensitive(elem, "type");
				if (cJSON_IsString(type) && (type->valuestring != NULL)) {
					typeval = cmparser_parser_get_fstype(type->valuestring);
					if (typeval == 0)
						continue;
					#ifdef _PRINTF_DEBUG_
					fprintf(stdout,"cmparser: fsconfig.fsmount.type = %d\n",typeval);
					#endif
				} else
					continue;

				from = cJSON_GetObjectItemCaseSensitive(elem, "from");
				if (!(cJSON_IsString(from) && (from->valuestring != NULL)))
					continue;

				to = cJSON_GetObjectItemCaseSensitive(elem, "to");
				if (!(cJSON_IsString(to) && (to->valuestring != NULL)))
					continue;

				fstype = cJSON_GetObjectItemCaseSensitive(elem, "fstype");
				if (!(cJSON_IsString(fstype) && (fstype->valuestring != NULL)))
					continue;

				option = cJSON_GetObjectItemCaseSensitive(elem, "option");
				if (!(cJSON_IsString(option) && (option->valuestring != NULL)))
					continue;

				// all data available
				p = (container_fsmount_elem_t*)malloc(sizeof(container_fsmount_elem_t));
				if (p == NULL)
					goto err_ret;

				memset(p, 0 , sizeof(container_fsmount_elem_t));
				dl_list_init(&p->list);
				p->type = typeval;
				p->from = strdup(from->valuestring);
				p->to = strdup(to->valuestring);
				p->fstype = strdup(fstype->valuestring);
				p->option = strdup(option->valuestring);
				#ifdef _PRINTF_DEBUG_
				fprintf(stdout,"cmparser: fsconfig.fsmount.type = %d, from = %s, to = %s, fstype = %s, option = %s\n",
							p->type, p->from, p->to, p->fstype, p->option);
				#endif

				dl_list_add_tail(&fc->fsmount.mountlist, &p->list);
			}
		}
	}

	return 0;

err_ret:
	{
		container_fsmount_elem_t *melem = NULL;
		// fs config
		while(dl_list_empty(&fc->fsmount.mountlist) == 0) {
			melem = dl_list_last(&fc->fsmount.mountlist, container_fsmount_elem_t, list);
			dl_list_del(&melem->list);
			free(melem->from);
			free(melem->to);
			free(melem->fstype);
			free(melem->option);
			free(melem);
		}
	}

	return result;
}

/**
 * Sub function for the static dev parse.
 * Shall not call from other than cmparser_parse_static_dev.
 *
 * @param [in]	str		string of devtype
 * @return int
 * @retval DEVICE_TYPE_DEVNODE	str is devnode
 * @retval DEVICE_TYPE_DEVDIR	str is devdir
 * @retval DEVICE_TYPE_GPIO 	str is gpio
 * @retval 0 NON
  */
static int cmparser_parser_get_devtype(const char *str)
{
	static const char devn[] = "devnode";
	static const char devd[] = "devdir";
	static const char gpio[] = "gpio";
	static const char iio[] = "iio";
	int ret = 0;

	if (strncmp(devn, str, sizeof(devn)) == 0)
		ret = DEVICE_TYPE_DEVNODE;
	else if (strncmp(devd, str, sizeof(devd)) == 0)
		ret = DEVICE_TYPE_DEVDIR;
	else if (strncmp(gpio, str, sizeof(gpio)) == 0)
		ret = DEVICE_TYPE_GPIO;
	else if (strncmp(iio, str, sizeof(iio)) == 0)
		ret = DEVICE_TYPE_IIO;

	return ret;
}

/**
 * Sub function for the gpio direction.
 * Shall not call from other than cmparser_parse_static_dev.
 *
 * @param [in]	str		string of gpio direction
 * @return int
 * @retval DEVGPIO_DIRECTION_IN	direction is input
 * @retval DEVGPIO_DIRECTION_OUT	direction is output, default low.
 * @retval DEVGPIO_DIRECTION_LOW	direction is output, default low.
 * @retval DEVGPIO_DIRECTION_HIGH	direction is output, default high.
 * @retval 0 NON
 */
static int cmparser_parser_get_gpiodirection(const char *str)
{
	static const char gpioin[] = "in";
	static const char gpioout[] = "out";
	static const char gpiolow[] = "low";
	static const char gpiohigh[] = "high";
	int ret = DEVGPIO_DIRECTION_DC;

	if (strncmp(gpioin, str, sizeof(gpioin)) == 0)
		ret = DEVGPIO_DIRECTION_IN;
	else if (strncmp(gpioout, str, sizeof(gpioout)) == 0)
		ret = DEVGPIO_DIRECTION_OUT;
	else if (strncmp(gpiolow, str, sizeof(gpiolow)) == 0)
		ret = DEVGPIO_DIRECTION_LOW;
	else if (strncmp(gpiohigh, str, sizeof(gpiohigh)) == 0)
		ret = DEVGPIO_DIRECTION_HIGH;

	return ret;
}

/**
 * parser for device-static section of container config.
 *
 * @param [out]	sdc	Pointer to pre-allocated container_static_device_t.
 * @param [in]	bc	Pointer to cJSON object of top of device-static section.
 * @return int
 * @retval  0 Success to parse.
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
static int cmparser_parse_static_dev(container_static_device_t *sdc, const cJSON *sd)
{
	// Get static array
	if (cJSON_IsArray(sd)) {
		cJSON *elem = NULL;

		cJSON_ArrayForEach(elem, sd) {
			if (cJSON_IsObject(elem)) {
				cJSON *type = NULL;
				int typeval = -1;

				type = cJSON_GetObjectItemCaseSensitive(elem, "type");
				if (cJSON_IsString(type) && (type->valuestring != NULL)) {
					typeval = cmparser_parser_get_devtype(type->valuestring);
					if (typeval == 0)
						continue;
					#ifdef _PRINTF_DEBUG_
					fprintf(stdout,"cmparser: deviceconfig.static_device.x.type = %d\n",typeval);
					#endif
				} else
					continue;

				if (typeval == DEVICE_TYPE_DEVNODE || typeval == DEVICE_TYPE_DEVDIR) {
					cJSON *from = NULL, *to = NULL;
					cJSON *devnode = NULL, *optional = NULL, *wideallow = NULL, *exclusive = NULL;
					container_static_device_elem_t *p = NULL;

					from = cJSON_GetObjectItemCaseSensitive(elem, "from");
					if (!(cJSON_IsString(from) && (from->valuestring != NULL)))
						continue;

					to = cJSON_GetObjectItemCaseSensitive(elem, "to");
					if (!(cJSON_IsString(to) && (to->valuestring != NULL)))
						continue;

					devnode = cJSON_GetObjectItemCaseSensitive(elem, "devnode");
					if (!(cJSON_IsString(devnode) && (devnode->valuestring != NULL)))
						continue;

					optional = cJSON_GetObjectItemCaseSensitive(elem, "optional");
					wideallow = cJSON_GetObjectItemCaseSensitive(elem, "wideallow");
					exclusive = cJSON_GetObjectItemCaseSensitive(elem, "exclusive");

					// all data available
					p = (container_static_device_elem_t*)malloc(sizeof(container_static_device_elem_t));
					if (p == NULL)
						goto err_ret;

					memset(p, 0 , sizeof(container_static_device_elem_t));
					dl_list_init(&p->list);

					p->type = typeval;
					p->from = strdup(from->valuestring);
					p->to = strdup(to->valuestring);
					p->devnode = strdup(devnode->valuestring);

					if (cJSON_IsNumber(optional))
						p->optional = optional->valueint;
					else
						p->optional = 0;	// default value

					if (cJSON_IsNumber(wideallow))
						p->wideallow = wideallow->valueint;
					else
						p->wideallow = 0;	// default value

					if (cJSON_IsNumber(exclusive))
						p->exclusive = exclusive->valueint;
					else
						p->exclusive = 0;	// default value

					#ifdef _PRINTF_DEBUG_
					fprintf(stdout,"cmparser: static_device.from = %s, to = %s, devnode = %s, optional = %d, wideallow = %d, exclusive = %d\n",
								p->from, p->to, p->devnode, p->optional, p->wideallow, p->exclusive);
					#endif

					dl_list_add_tail(&sdc->static_devlist, &p->list);

				} else if (typeval == DEVICE_TYPE_GPIO) {
					cJSON *from = NULL, *to = NULL;
					cJSON *port = NULL, *direction = NULL;
					int directionval = 0;
					container_static_gpio_elem_t *p = NULL;

					port = cJSON_GetObjectItemCaseSensitive(elem, "port");
					if (!cJSON_IsNumber(port) || (port->valueint < 0))
						continue;

					direction = cJSON_GetObjectItemCaseSensitive(elem, "direction");
					if (cJSON_IsString(direction) && direction->valuestring != NULL) {
						directionval = cmparser_parser_get_gpiodirection(direction->valuestring);
					} else {
						directionval = 0;
					}

					from = cJSON_GetObjectItemCaseSensitive(elem, "from");
					if (!(cJSON_IsString(from) && (from->valuestring != NULL)))
						continue;

					to = cJSON_GetObjectItemCaseSensitive(elem, "to");
					if (!(cJSON_IsString(to) && (to->valuestring != NULL)))
						continue;

					// all data available
					p = (container_static_gpio_elem_t*)malloc(sizeof(container_static_gpio_elem_t));
					if (p == NULL)
						goto err_ret;

					memset(p, 0 , sizeof(container_static_gpio_elem_t));
					dl_list_init(&p->list);

					p->type = typeval;
					p->port = port->valueint;
					p->portdirection = directionval;
					p->from = strdup(from->valuestring);
					p->to = strdup(to->valuestring);

					#ifdef _PRINTF_DEBUG_
					fprintf(stdout,"cmparser: gpio.portnum = %d, direction = %d, from = %s, to = %s\n",
								p->port, p->portdirection, p->from, p->to);
					#endif

					dl_list_add_tail(&sdc->static_gpiolist, &p->list);

				} else if (typeval == DEVICE_TYPE_IIO) {
					cJSON *sysfrom = NULL, *systo = NULL, *devfrom = NULL, *devto = NULL;
					cJSON *devnode = NULL, *optional = NULL;
					char *pdevfrom = NULL, *pdevto = NULL, *pdevnode = NULL;
					container_static_iio_elem_t *p = NULL;

					sysfrom = cJSON_GetObjectItemCaseSensitive(elem, "sysfrom");
					if (!(cJSON_IsString(sysfrom) && (sysfrom->valuestring != NULL))) {
						// sysfrom is mandatory, skip.
						continue;
					}

					systo = cJSON_GetObjectItemCaseSensitive(elem, "systo");
					if (!(cJSON_IsString(systo) && (systo->valuestring != NULL))) {
						// systo is mandatory, skip.
						continue;
					}

					devfrom = cJSON_GetObjectItemCaseSensitive(elem, "devfrom");
					if (cJSON_IsString(devfrom) && (devfrom->valuestring != NULL)) {
						// devfrom is optional
						pdevfrom = devfrom->valuestring;
					}

					devto = cJSON_GetObjectItemCaseSensitive(elem, "devto");
					if (cJSON_IsString(devto) && (devto->valuestring != NULL)) {
						// devto is optional
						pdevto = devto->valuestring;
					}

					devnode = cJSON_GetObjectItemCaseSensitive(elem, "devnode");
					if (cJSON_IsString(devnode) && (devnode->valuestring != NULL)) {
						// devnode is optional
						pdevnode = devnode->valuestring;
					}

					optional = cJSON_GetObjectItemCaseSensitive(elem, "optional");

					// all data available
					p = (container_static_iio_elem_t*)malloc(sizeof(container_static_iio_elem_t));
					if (p == NULL)
						goto err_ret;

					memset(p, 0 , sizeof(container_static_iio_elem_t));
					dl_list_init(&p->list);

					p->type = typeval;
					p->sysfrom = strdup(sysfrom->valuestring);
					p->systo = strdup(systo->valuestring);
					if (pdevfrom != NULL)
						p->devfrom = strdup(pdevfrom);

					if (pdevto != NULL)
						p->devto = strdup(pdevto);

					if (pdevnode != NULL)
						p->devnode = strdup(pdevnode);

					if (cJSON_IsNumber(optional))
						p->optional = optional->valueint;
					else
						p->optional = 0;	// default value

					#ifdef _PRINTF_DEBUG_
					fprintf(stdout,"cmparser: iio sysfrom = %s, systo = %s, devfrom = %s, devto = %s, devnode = %s, optional = %d\n",
								p->sysfrom, p->systo, p->devfrom, p->devto, p->devnode, p->optional);
					#endif

					dl_list_add_tail(&sdc->static_iiolist, &p->list);

				}
			}
		}
	}

	return 0;

err_ret:
	{
		container_static_device_elem_t *delem = NULL;
		// static device config
		while(dl_list_empty(&sdc->static_devlist) == 0) {
			delem = dl_list_last(&sdc->static_devlist, container_static_device_elem_t, list);
			dl_list_del(&delem->list);
			free(delem->from);
			free(delem->to);
			free(delem->devnode);
			free(delem);
		}

		container_static_gpio_elem_t *gpioelem = NULL;
		// static gpio config
		while(dl_list_empty(&sdc->static_gpiolist) == 0) {
			gpioelem = dl_list_last(&sdc->static_gpiolist, container_static_gpio_elem_t, list);
			dl_list_del(&gpioelem->list);
			free(gpioelem->from);
			free(gpioelem->to);
			free(gpioelem);
		}

		container_static_iio_elem_t *iioelem = NULL;
		// static iio config
		while(dl_list_empty(&sdc->static_iiolist) == 0) {
			iioelem = dl_list_last(&sdc->static_iiolist, container_static_iio_elem_t, list);
			dl_list_del(&iioelem->list);
			free(iioelem->sysfrom);
			free(iioelem->systo);
			free(iioelem->devfrom);
			free(iioelem->devto);
			free(iioelem->devnode);
			free(iioelem);
		}
	}

	return -3;
}
/**
 * parser for device-dynamic section of container config.
 *
 * @param [out]	ddc	Pointer to pre-allocated container_dynamic_device_t.
 * @param [in]	dd	Pointer to cJSON object of top of device-dynamic section.
 * @return int
 * @retval  0 Success to parse.
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
static int cmparser_parse_dynamic_dev(container_dynamic_device_t *ddc, const cJSON *dd)
{
	// Get dynamic array
	if (cJSON_IsArray(dd)) {
		cJSON *elem = NULL;

		cJSON_ArrayForEach(elem, dd) {
			if (cJSON_IsObject(elem)) {
				cJSON *devpath = NULL, *subsystem = NULL, *devtype = NULL, *mode = NULL;
				int imode = 3;
				container_dynamic_device_elem_t *p = NULL;

				devpath = cJSON_GetObjectItemCaseSensitive(elem, "devpath");
				if (!(cJSON_IsString(devpath) && (devpath->valuestring != NULL)))
					continue;

				subsystem = cJSON_GetObjectItemCaseSensitive(elem, "subsystem");
				if (!(cJSON_IsString(subsystem) && (subsystem->valuestring != NULL)))
					continue;

				devtype = cJSON_GetObjectItemCaseSensitive(elem, "devtype");
				if (!(cJSON_IsString(devtype) && (devtype->valuestring != NULL)))
					continue;

				//mode: 0: cgroup allow, 1: cgroup allow and add dev node, 2: cgroup allow and uevent injection, 3: all
				mode = cJSON_GetObjectItemCaseSensitive(elem, "mode");
				if (cJSON_IsNumber(mode)) {
					imode = mode->valueint;
					if (imode < 0 || 3 < imode)
						imode = 3;
				}

				// all data available
				p = (container_dynamic_device_elem_t*)malloc(sizeof(container_dynamic_device_elem_t));
				if (p == NULL)
					goto err_ret;

				memset(p, 0 , sizeof(container_dynamic_device_elem_t));
				dl_list_init(&p->list);
				dl_list_init(&p->device_list);

				p->devpath = strdup(devpath->valuestring);
				p->subsystem = strdup(subsystem->valuestring);
				p->devtype = strdup(devtype->valuestring);
				p->mode = imode;

				#ifdef _PRINTF_DEBUG_
				fprintf(stdout,"cmparser: dynamic_device.devpath = %s, subsystem = %s, devtype = %s, mode = %d\n",
							p->devpath, p->subsystem, p->devtype, p->mode);
				#endif

				dl_list_add_tail(&ddc->dynamic_devlist, &p->list);
			}
		}
	}

	return 0;

err_ret:
	{
		container_dynamic_device_elem_t *delem = NULL;
		// dynamic device config
		while(dl_list_empty(&ddc->dynamic_devlist) == 0) {
			delem = dl_list_last(&ddc->dynamic_devlist, container_dynamic_device_elem_t, list);
			dl_list_del(&delem->list);
			free(delem->devpath);
			free(delem->subsystem);
			free(delem->devtype);
			free(delem);
		}
	}

	return -3;
}

/**
 * parser for device-dynamic section of container config.
 *
 * @param [out]	dc	Pointer to pre-allocated container_deviceconfig_t.
 * @param [in]	dd	Pointer to cJSON object of top of device-dynamic section.
 * @return int
 * @retval  0 Success to parse.
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
int cmparser_parse_device(container_deviceconfig_t *dc, const cJSON *dev)
{
	cJSON *static_device = NULL;
	cJSON *dynamic_device = NULL;

	// Get static device data
	static_device = cJSON_GetObjectItemCaseSensitive(dev, "static");
	if (cJSON_IsArray(static_device)) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: static device entry found\n");
		#endif
		(void) cmparser_parse_static_dev(&dc->static_device, static_device);
	}
	// Static device is not mandatory

	// Get static device data
	dynamic_device = cJSON_GetObjectItemCaseSensitive(dev, "dynamic");
	if (cJSON_IsArray(dynamic_device)) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: dynamic device entry found\n");
		#endif
		(void) cmparser_parse_dynamic_dev(&dc->dynamic_device, dynamic_device);
	}
	// Dynamic device is not mandatory

	return 0;
}
/**
 * Sub function for the static netif.
 * Shall not call from other than cmparser_parse_static_netif.
 *
 * @param [in]	str		string of netif type
 * @return int
 * @retval STATICNETIF_VETH	netif is veth
 * @retval 0 NON
 */
static int cmparser_parser_get_netiftype(const char *str)
{
	static const char veth[] = "veth";
	int ret = 0;

	if (strncmp(veth, str, sizeof(veth)) == 0)
		ret = STATICNETIF_VETH;

	return ret;
}
/**
 * Read json string with memory allocation
 *
 * @param [in]	file		Full file path for json file
 * @return char*
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
static void* cmparser_parse_static_netif_veth_create(cJSON *param)
{
	cJSON *name = NULL, *link = NULL, *flags = NULL, *hwaddr = NULL, *mode = NULL;
	cJSON *address = NULL, *gateway = NULL;
	char *pname = NULL, *plink = NULL, *pflags = NULL, *phwaddr = NULL, *pmode = NULL;
	char *paddress = NULL, *pgateway = NULL;
	netif_elem_veth_t *pveth = NULL;
	void *vp = NULL;


	pveth = (netif_elem_veth_t*)malloc(sizeof(netif_elem_veth_t));
	if (pveth == NULL) {
		return NULL;
	}

	link = cJSON_GetObjectItemCaseSensitive(param, "link");
	if (cJSON_IsString(link) && (link->valuestring != NULL)) {
		plink = strdup(link->valuestring);
	} else {
		//link is mandatory
		free(pveth);
		return NULL;
	}

	name = cJSON_GetObjectItemCaseSensitive(param, "name");
	if (cJSON_IsString(name) && (name->valuestring != NULL)) {
		pname = strdup(name->valuestring);
	}

	flags = cJSON_GetObjectItemCaseSensitive(param, "flags");
	if (cJSON_IsString(flags) && (flags->valuestring != NULL)) {
		pflags = strdup(flags->valuestring);
	}

	hwaddr = cJSON_GetObjectItemCaseSensitive(param, "hwaddr");
	if (cJSON_IsString(hwaddr) && (hwaddr->valuestring != NULL)) {
		phwaddr = strdup(hwaddr->valuestring);
	}

	mode = cJSON_GetObjectItemCaseSensitive(param, "mode");
	if (cJSON_IsString(mode) && (mode->valuestring != NULL)) {
		pmode = strdup(mode->valuestring);
	}

	address = cJSON_GetObjectItemCaseSensitive(param, "address");
	if (cJSON_IsString(address) && (address->valuestring != NULL)) {
		paddress = strdup(address->valuestring);
	}

	gateway = cJSON_GetObjectItemCaseSensitive(param, "gateway");
	if (cJSON_IsString(gateway) && (gateway->valuestring != NULL)) {
		pgateway = strdup(gateway->valuestring);
	}

	pveth->link = plink;
	pveth->name = pname;
	pveth->flags = pflags;
	pveth->hwaddr = phwaddr;
	pveth->mode = pmode;
	pveth->address = paddress;
	pveth->gateway = pgateway;

	vp = (void*)pveth;

	return vp;
}
/**
 * Read json string with memory allocation
 *
 * @param [in]	file		Full file path for json file
 * @return char*
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
static int cmparser_parse_static_netif_veth_free(void *p)
{
	netif_elem_veth_t *pveth = NULL;

	if (p == NULL)
		return 0;

	pveth = (netif_elem_veth_t*)p;
	free(pveth->link);
	free(pveth->flags);
	free(pveth->hwaddr);
	free(pveth->mode);
	free(pveth->address);
	free(pveth->gateway);
	free(pveth);

	return 0;
}
/**
 * Read json string with memory allocation
 *
 * @param [in]	file		Full file path for json file
 * @return char*
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
static int cmparser_parse_static_netif(container_static_netif_t *snif, const cJSON *sni)
{
	int result = -1;

	// Get netif data
	if (cJSON_IsArray(sni)) {
		cJSON *elem = NULL;

		cJSON_ArrayForEach(elem, sni) {
			if (cJSON_IsObject(elem)) {
				cJSON *type = NULL, *param = NULL;
				container_static_netif_elem_t *p = NULL;
				void *vp = NULL;
				int iftype = 0;

				type = cJSON_GetObjectItemCaseSensitive(elem, "type");
				if (!(cJSON_IsString(type) && (type->valuestring != NULL)))
					continue;

				iftype = cmparser_parser_get_netiftype(type->valuestring);
				if (iftype > 0) {
					param = cJSON_GetObjectItemCaseSensitive(elem, "param");
					if (!(cJSON_IsObject(param)))
						continue;

					if (iftype == STATICNETIF_VETH) {
						vp = cmparser_parse_static_netif_veth_create(param);
						if (vp == NULL)
							continue;
					}

					// all data available
					p = (container_static_netif_elem_t*)malloc(sizeof(container_static_netif_elem_t));
					if (p == NULL) {
						result = -3;
						goto err_ret;
					}

					p->type = iftype;
					p->setting = vp;
					dl_list_init(&p->list);
					dl_list_add_tail(&snif->static_netiflist, &p->list);
				}
			}
		}
	}

	return 0;

err_ret:
	{
		container_static_netif_elem_t *selem = NULL;
		// dynamic device config
		while(dl_list_empty(&snif->static_netiflist) == 0) {
			selem = dl_list_last(&snif->static_netiflist, container_static_netif_elem_t, list);
			dl_list_del(&selem->list);
			if (selem->type == STATICNETIF_VETH)
				cmparser_parse_static_netif_veth_free((void *)selem->setting);

			free(selem);
		}
	}

	return -3;
}
/**
 * Read json string with memory allocation
 *
 * @param [in]	file		Full file path for json file
 * @return char*
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
static int cmparser_parse_dynamic_netif(container_dynamic_netif_t *dnif, const cJSON *dni)
{
	// Get netif data
	if (cJSON_IsArray(dni)) {
		cJSON *elem = NULL;

		cJSON_ArrayForEach(elem, dni) {
			if (cJSON_IsObject(elem)) {
				cJSON *ifname = NULL;
				container_dynamic_netif_elem_t *p = NULL;

				ifname = cJSON_GetObjectItemCaseSensitive(elem, "ifname");
				if (!(cJSON_IsString(ifname) && (ifname->valuestring != NULL)))
					continue;

				// all data available
				p = (container_dynamic_netif_elem_t*)malloc(sizeof(container_dynamic_netif_elem_t));
				if (p == NULL)
					goto err_ret;

				memset(p, 0 , sizeof(container_dynamic_netif_elem_t));
				dl_list_init(&p->list);

				p->ifname = strdup(ifname->valuestring);

				#ifdef _PRINTF_DEBUG_
				fprintf(stdout,"cmparser: dynamic_netif.ifname = %s\n", p->ifname);
				#endif

				dl_list_add_tail(&dnif->dynamic_netiflist, &p->list);
			}
		}
	}

	return 0;

err_ret:
	{
		container_dynamic_netif_elem_t *delem = NULL;
		// dynamic device config
		while(dl_list_empty(&dnif->dynamic_netiflist) == 0) {
			delem = dl_list_last(&dnif->dynamic_netiflist, container_dynamic_netif_elem_t, list);
			dl_list_del(&delem->list);
			free(delem->ifname);
			free(delem);
		}
	}

	return -3;
}
/**
 * Read json string with memory allocation
 *
 * @param [in]	file		Full file path for json file
 * @return char*
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
int cmparser_parse_netif(container_netifconfig_t *nc, const cJSON *nif)
{
	cJSON *static_netif = NULL;
	cJSON *dynamic_netif = NULL;

	// Get static device data
	static_netif = cJSON_GetObjectItemCaseSensitive(nif, "static");
	if (cJSON_IsArray(static_netif)) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: static network interface entry found\n");
		#endif
		(void) cmparser_parse_static_netif(&nc->static_netif, static_netif);
	}
	// Static netif is not mandatory

	// Get static device data
	dynamic_netif = cJSON_GetObjectItemCaseSensitive(nif, "dynamic");
	if (cJSON_IsArray(dynamic_netif)) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"cmparser: dynamic network interface found\n");
		#endif
		(void) cmparser_parse_dynamic_netif(&nc->dynamic_netif, dynamic_netif);
	}
	// Dynamic netif is not mandatory

	return 0;
}
/**
 * Read json string with memory allocation
 *
 * @param [in]	file		Full file path for json file
 * @return int
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
int cmparser_create_from_file(container_config_t **cc, const char *file)
{
	cJSON *json = NULL;
	char *jsonstring = NULL;
	container_config_t *ccfg = NULL;
	int result = -1;
	int ret = -1;

	if (cc == NULL || file == NULL)
		return -1;

	jsonstring = cmparser_read_jsonstring(file);
	if (jsonstring == NULL) {
		result = -1;
		goto err_ret;
	}

	json = cJSON_Parse(jsonstring);
	if (json == NULL) {
		result = -2;
		goto err_ret;
	}

	ccfg = (container_config_t*)malloc(sizeof(container_config_t));
	if (ccfg == NULL) {
		result = -3;
		goto err_ret;
	}
	memset(ccfg, 0, sizeof(container_config_t));
	dl_list_init(&ccfg->baseconfig.extradisk_list);
	dl_list_init(&ccfg->baseconfig.envlist);
	dl_list_init(&ccfg->resourceconfig.resource.resourcelist);
	dl_list_init(&ccfg->fsconfig.fsmount.mountlist);
	dl_list_init(&ccfg->deviceconfig.static_device.static_devlist);
	dl_list_init(&ccfg->deviceconfig.static_device.static_gpiolist);
	dl_list_init(&ccfg->deviceconfig.static_device.static_iiolist);
	dl_list_init(&ccfg->deviceconfig.dynamic_device.dynamic_devlist);
	dl_list_init(&ccfg->netifconfig.static_netif.static_netiflist);
	dl_list_init(&ccfg->netifconfig.dynamic_netif.dynamic_netiflist);


	// Get name data
	{
		const cJSON *name = NULL;
		name = cJSON_GetObjectItemCaseSensitive(json, "name");
		if (cJSON_IsString(name) && (name->valuestring != NULL)) {
			ccfg->name = strdup(name->valuestring);
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"\ncmparser: start %s\n", ccfg->name);
			#endif
		} else {
			result = -2;
			goto err_ret;
		}
	}

	// Get base data
	{
		const cJSON *base = NULL;
		base = cJSON_GetObjectItemCaseSensitive(json, "base");
		if (cJSON_IsObject(base)) {
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: base entry found\n");
			#endif
			ret = cmparser_parse_base(&ccfg->baseconfig, base);
		} else {
			// Mandatory
			result = -2;
			goto err_ret;
		}
	}

	// Get resource data
	{
		const cJSON *resource = NULL;
		resource = cJSON_GetObjectItemCaseSensitive(json, "resource");
		if (cJSON_IsArray(resource)) {
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: resource entry found\n");
			#endif
			ret = cmparser_parse_resource(&ccfg->resourceconfig, resource);
		}

		// Not mandatory
	}

	// Get fs data
	{
		const cJSON *fs = NULL;
		fs = cJSON_GetObjectItemCaseSensitive(json, "fs");
		if (cJSON_IsObject(fs)) {
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: fs entry found\n");
			#endif
			ret = cmparser_parse_fs(&ccfg->fsconfig, fs);
		} else {
			// Mandatory
			result = -2;
			goto err_ret;
		}
	}

	// Get device data
	{
		const cJSON *device = NULL;
		device = cJSON_GetObjectItemCaseSensitive(json, "device");
		if (cJSON_IsObject(device)) {
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: device entry found\n");
			#endif
			ret = cmparser_parse_device(&ccfg->deviceconfig, device);
		} else {
			// Mandatory
			result = -2;
			goto err_ret;
		}
	}

	// Get network data
	{
		const cJSON *network = NULL;
		network = cJSON_GetObjectItemCaseSensitive(json, "network");
		if (cJSON_IsObject(network)) {
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"cmparser: network entry found\n");
			#endif
			ret = cmparser_parse_netif(&ccfg->netifconfig, network);

		}
		// Not mandatory
	}

	cJSON_Delete(json);
	cmparser_release_jsonstring(jsonstring);

	(*cc) = ccfg;

	return 0;

err_ret:
	if (json != NULL)
		cJSON_Delete(json);

	if (jsonstring != NULL)
		cmparser_release_jsonstring(jsonstring);

	cmparser_release_config(ccfg);

	return result;
}
/**
 * Release container config allocated by cmparser_create_from_file

 *
 * @param [in]	cc		Container config
 * @return void
 */
void cmparser_release_config(container_config_t *cc)
{

	if (cc == NULL)
		return;

	// static device config
	{
		container_static_device_elem_t *delem = NULL;
		container_static_gpio_elem_t *gpioelem = NULL;
		container_static_iio_elem_t *iioelem = NULL;

		while(dl_list_empty(&cc->deviceconfig.static_device.static_devlist) == 0) {
			delem = dl_list_last(&cc->deviceconfig.static_device.static_devlist, container_static_device_elem_t, list);
			dl_list_del(&delem->list);
			free(delem->from);
			free(delem->to);
			free(delem->devnode);
			free(delem);
		}

		while(dl_list_empty(&cc->deviceconfig.static_device.static_gpiolist) == 0) {
			gpioelem = dl_list_last(&cc->deviceconfig.static_device.static_gpiolist, container_static_gpio_elem_t, list);
			dl_list_del(&gpioelem->list);
			free(gpioelem->from);
			free(gpioelem->to);
			free(gpioelem);
		}

		while(dl_list_empty(&cc->deviceconfig.static_device.static_iiolist) == 0) {
			iioelem = dl_list_last(&cc->deviceconfig.static_device.static_iiolist, container_static_iio_elem_t, list);
			dl_list_del(&iioelem->list);
			free(iioelem->sysfrom);
			free(iioelem->systo);
			free(iioelem->devfrom);
			free(iioelem->devto);
			free(iioelem->devnode);
			free(iioelem);
		}
	}

	// dynamic device config
	{
		container_dynamic_device_elem_t *delem = NULL;
		// dynamic device config
		while(dl_list_empty(&cc->deviceconfig.dynamic_device.dynamic_devlist) == 0) {
			delem = dl_list_last(&cc->deviceconfig.dynamic_device.dynamic_devlist, container_dynamic_device_elem_t, list);
			dl_list_del(&delem->list);
			free(delem->devpath);
			free(delem->subsystem);
			free(delem->devtype);
			free(delem);
		}
	}

	// static net if config
	{
		container_static_netif_elem_t *selem = NULL;
		// static net if config
		while(dl_list_empty(&cc->netifconfig.static_netif.static_netiflist) == 0) {
			selem = dl_list_last(&cc->netifconfig.static_netif.static_netiflist, container_static_netif_elem_t, list);
			dl_list_del(&selem->list);
			if (selem->type == STATICNETIF_VETH)
				cmparser_parse_static_netif_veth_free((void *)selem->setting);

			free(selem);
		}
	}

	// dynamic net if config
	{
		container_dynamic_netif_elem_t *delem = NULL;
		// dynamic net if config
		while(dl_list_empty(&cc->netifconfig.dynamic_netif.dynamic_netiflist) == 0) {
			delem = dl_list_last(&cc->netifconfig.dynamic_netif.dynamic_netiflist, container_dynamic_netif_elem_t, list);
			dl_list_del(&delem->list);
			free(delem->ifname);
			free(delem);
		}
	}

	// fs config
	{
		container_fsmount_elem_t *melem = NULL;

		while(dl_list_empty(&cc->fsconfig.fsmount.mountlist) == 0) {
			melem = dl_list_last(&cc->fsconfig.fsmount.mountlist, container_fsmount_elem_t, list);
			dl_list_del(&melem->list);
			free(melem->from);
			free(melem->to);
			free(melem->fstype);
			free(melem->option);
			free(melem);
		}
	}

	// resource config
	{
		container_resource_elem_t *melem = NULL;
		// resource config
		while(dl_list_empty(&cc->resourceconfig.resource.resourcelist) == 0) {
			melem = dl_list_last(&cc->resourceconfig.resource.resourcelist, container_resource_elem_t, list);
			dl_list_del(&melem->list);
			free(melem->object);
			free(melem->value);
			free(melem);
		}
	}

	// base config
	{
		container_baseconfig_env_t *env = NULL;
		container_baseconfig_extradisk_t *exdisk = NULL;

		while(dl_list_empty(&cc->baseconfig.envlist) == 0) {
			env = dl_list_last(&cc->baseconfig.envlist, container_baseconfig_env_t, list);
			dl_list_del(&env->list);
			free(env->envstring);
			free(env);
		}

		free(cc->baseconfig.cap.drop);
		free(cc->baseconfig.cap.keep);

		free(cc->baseconfig.lifecycle.halt);
		free(cc->baseconfig.lifecycle.reboot);

		while(dl_list_empty(&cc->baseconfig.extradisk_list) == 0) {
			exdisk = dl_list_last(&cc->baseconfig.extradisk_list, container_baseconfig_extradisk_t, list);
			dl_list_del(&exdisk->list);
			free(exdisk->blockdev[0]);
			free(exdisk->blockdev[1]);
			free(exdisk->filesystem);
			free(exdisk->to);
			free(exdisk->from);
			free(exdisk);
		}

		free(cc->baseconfig.rootfs.blockdev[0]);
		free(cc->baseconfig.rootfs.blockdev[1]);
		free(cc->baseconfig.rootfs.filesystem);
		free(cc->baseconfig.rootfs.path);
		free(cc->baseconfig.role);
	}

	// global
	free(cc->name);
	free(cc);
}
