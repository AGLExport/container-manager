/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	parser-manager.c
 * @brief	config file parser using cjson
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include <cjson/cJSON.h>
#include "list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser/parser-common.h"
#include "parser/parser-manager.h"

#undef _PRINTF_DEBUG_

/**
 * Sub function for the mount type parse.
 * Shall not call from other than cmparser_manager_parse_operation_mount.
 *
 * @param [in]	str		string of fstype
 * @return int
 * @retval MANAGER_MOUNT_TYPE_PRE	str is "pre"
 * @retval MANAGER_MOUNT_TYPE_POST	str is "post"
 * @retval MANAGER_MOUNT_TYPE_DELAYED	str is "delayed"
 * @retval 0 NON
 */
static int cmparser_manager_parser_get_fstype(const char *str)
{
	static const char cpre[] = "pre";
	static const char cpost[] = "post";
	static const char cdelayed[] = "delayed";
	int ret = 0;

	if (strncmp(cpre, str, sizeof(cpre)) == 0) {
		ret = MANAGER_MOUNT_TYPE_PRE;
	} else if (strncmp(cpost, str, sizeof(cpost)) == 0) {
		ret = MANAGER_MOUNT_TYPE_POST;
	} else if (strncmp(cdelayed, str, sizeof(cdelayed)) == 0) {
		ret = MANAGER_MOUNT_TYPE_DELAYED;
	} else {
		//Unknown str, set NON - error
		ret = 0;
	}

	return ret;
}
/**
 * Sub function for the disk mount mode parse.
 * Shall not call from other than cmparser_manager_parse_operation_mount.
 *
 * @param [in]	str		string of fstype
 * @return int
 * @retval MANAGER_DISKMOUNT_TYPE_RO	str is "ro" or other
 * @retval MANAGER_DISKMOUNT_TYPE_RW	str is "rw"
 */
static int cmparser_manager_parser_get_diskmountmode(const char *str)
{
	static const char read_only[] = "ro";
	static const char read_write[] = "rw";
	int ret = MANAGER_DISKMOUNT_TYPE_RO;

	if (strncmp(read_only, str, sizeof(read_only)) == 0) {
		ret = MANAGER_DISKMOUNT_TYPE_RO;
	} else if (strncmp(read_write, str, sizeof(read_write)) == 0) {
		ret = MANAGER_DISKMOUNT_TYPE_RW;
	} else {
		// unknow str, select RO.
		ret = MANAGER_DISKMOUNT_TYPE_RO;
	}

	return ret;
}
/**
 * Sub function for the disk mount fail operation parse.
 * Shall not call from other than cmparser_manager_parse_operation_mount.
 *
 * @param [in]	str		string of fstype
 * @return int
 * @retval MANAGER_DISKREDUNDANCY_TYPE_FAILOVER	str is "failover" or other
 * @retval MANAGER_DISKREDUNDANCY_TYPE_AB	str is "ab"
 */
static int cmparser_manager_parser_get_diskmountfailop(const char *str)
{
	static const char failover[] = "failover";
	static const char ab[] = "ab";
	static const char fsck[] = "fsck";
	static const char mkfs[] = "mkfs";
	int ret = MANAGER_DISKREDUNDANCY_TYPE_FAILOVER;

	if (strncmp(failover, str, sizeof(failover)) == 0) {
		ret = MANAGER_DISKREDUNDANCY_TYPE_FAILOVER;
	} else if (strncmp(ab, str, sizeof(ab)) == 0) {
		ret = MANAGER_DISKREDUNDANCY_TYPE_AB;
	} else if (strncmp(fsck, str, sizeof(fsck)) == 0) {
		ret = MANAGER_DISKREDUNDANCY_TYPE_FSCK;
	} else if (strncmp(mkfs, str, sizeof(mkfs)) == 0) {
		ret = MANAGER_DISKREDUNDANCY_TYPE_MKFS;
	} else {
		// unknow str, select FAILOVER.
		ret = MANAGER_DISKREDUNDANCY_TYPE_FAILOVER;
	}

	return ret;
}
/**
 * Sub function for the operation mount config parser.
 *
 * @param [out]	cmom	Pointer to pre-allocated container_manager_operation_mount_t.
 * @param [in]	mount	Pointer to cJSON object of top of base section.
 * @return int
 * @retval  0 Success to parse.
 * @retval -1 Json file error.
 * @retval -2 Json file parse error.(Reserve)
 * @retval -3 Memory allocation error.
 */
static int cmparser_manager_parse_operation_mount(container_manager_operation_mount_t *cmom, const cJSON *mount)
{
	int result = -1;
	int index = 0;
	cJSON *elem = NULL;

	cJSON_ArrayForEach(elem, mount) {
		cJSON *type = NULL, *to = NULL,  *blockdev = NULL;
		cJSON *filesystem = NULL, *mode = NULL, *option = NULL, *redundancy = NULL;
		container_manager_operation_mount_elem_t *mount_elem = NULL;
		int mntmode = 0, mntredundancy = 0;
		char *bdev[2], *fsstr = NULL, *optionstr = NULL;
		bdev[0] = NULL;
		bdev[1] = NULL;

		type = cJSON_GetObjectItemCaseSensitive(elem, "type");
		if (cJSON_IsString(type) && (type->valuestring != NULL)) {
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"cmparser_manager: mount type = %s\n",from->valuestring);
			#endif
			;
		} else {
			// Mandatory value, drop this entry.
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			(void) fprintf(stderr,"[CM CRITICAL ERROR] cmparser_manager: mount type not set. It's mandatory value. drop entry\n");
			#endif
			continue;
		}

		to = cJSON_GetObjectItemCaseSensitive(elem, "to");
		if (cJSON_IsString(to) && (to->valuestring != NULL)) {
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"cmparser_manager: mount to = %s\n",to->valuestring);
			#endif
			;
		} else {
			// Mandatory value, drop this entry.
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			(void) fprintf(stderr,"[CM CRITICAL ERROR] cmparser_manager: mount to not set. It's mandatory value. drop entry\n");
			#endif
			continue;
		}

		filesystem = cJSON_GetObjectItemCaseSensitive(elem, "filesystem");
		if (cJSON_IsString(filesystem) && (filesystem->valuestring != NULL)) {
			fsstr = filesystem->valuestring;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"cmparser_manager: mount filesystem = %s\n",filesystem->valuestring);
			#endif
		} else {
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			(void) fprintf(stderr,"[CM CRITICAL ERROR] cmparser_manager: mount filesystem not set. It's mandatory value. drop entry\n");
			#endif
			continue;
		}

		mode = cJSON_GetObjectItemCaseSensitive(elem, "mode");
		if (cJSON_IsString(mode) && (mode->valuestring != NULL)) {
			mntmode = cmparser_manager_parser_get_diskmountmode(mode->valuestring);
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"cmparser_manager: mount mode = %s\n",mode->valuestring);
			#endif
		} else {
			// When don't have disk mount mode setting, It's use ro mount as a default.
			mntmode = MANAGER_DISKMOUNT_TYPE_RO;
		}

		option = cJSON_GetObjectItemCaseSensitive(elem, "option");
		if (cJSON_IsString(option) && (option->valuestring != NULL)) {
			optionstr = option->valuestring;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"cmparser_manager: mount option = %s\n",optionstr);
			#endif
		} else {
			// When don't have disk option setting, It's use default for filesystem.
			optionstr = NULL;
		}

		redundancy = cJSON_GetObjectItemCaseSensitive(elem, "redundancy");
		if (cJSON_IsString(redundancy) && (redundancy->valuestring != NULL)) {
			mntredundancy = cmparser_manager_parser_get_diskmountfailop(redundancy->valuestring);
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"cmparser_manager: mount redundancy = %s\n",redundancy->valuestring);
			#endif
		} else {
			// When don't have disk mount mode setting, It's use failover as a default.
			mntredundancy = MANAGER_DISKREDUNDANCY_TYPE_FAILOVER;
		}

		blockdev = cJSON_GetObjectItemCaseSensitive(elem, "blockdev");
		if (cJSON_IsArray(blockdev)) {
			cJSON *dev = NULL;
			int i = 0;

			cJSON_ArrayForEach(dev, blockdev) {
				if (i < 2) {
					if (cJSON_IsString(dev) && (dev->valuestring != NULL)) {
						bdev[i] = dev->valuestring;
						#ifdef _PRINTF_DEBUG_
						(void) fprintf(stdout,"cmparser_manager: mount blockdev[%d] = %s\n", i, bdev[i]);
						#endif
					} else {
						bdev[i] = NULL;
						#ifdef _PRINTF_DEBUG_
						(void) fprintf(stdout,"cmparser_manager: mount blockdev[%d] set default value = NULL\n", i);
						#endif
					}
				}
				i++;
			}
		}

		if(bdev[0] == NULL) {
			// Mandatory value, drop this entry.
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			(void) fprintf(stderr,"[CM CRITICAL ERROR] cmparser_manager: mount blockdev[0] not set. It's mandatory value. drop entry\n");
			#endif
			continue;
		}

		// When no error, create add list
		mount_elem = (container_manager_operation_mount_elem_t*)malloc(sizeof(container_manager_operation_mount_elem_t));
		if (mount_elem == NULL) {
			result = -3;
			goto err_ret;
		}
		(void) memset(mount_elem, 0 , sizeof(container_manager_operation_mount_elem_t));

		dl_list_init(&mount_elem->list);
		mount_elem->type = cmparser_manager_parser_get_fstype(type->valuestring);
		mount_elem->to = strdup(to->valuestring);
		mount_elem->filesystem = strdup(fsstr);
		mount_elem->mode = mntmode;
		mount_elem->redundancy = mntredundancy;
		if (optionstr != NULL) {
			mount_elem->option = strdup(optionstr);
		}
		mount_elem->blockdev[0] = strdup(bdev[0]);
		if (bdev[1] != NULL) {
			mount_elem->blockdev[1] = strdup(bdev[1]);
		}
		mount_elem->index = index;
		index++;

		dl_list_add_tail(&cmom->mount_list, &mount_elem->list);

		result = 0;
	}

	return result;

err_ret:

	return result;
}

/**
 * Create container manager config object from json file.
 *
 * @param [out]	cm		Double pointer to container_manager_config_t to out container_manager_config_t object.
 * @param [in]	file	Full file path for json file
 * @return int
 * @retval  0 Success create container_config_t object.
 * @retval -1 Argument error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 */
int cmparser_manager_create_from_file(container_manager_config_t **cm, const char *file)
{
	cJSON *json = NULL;
	char *jsonstring = NULL;
	container_manager_config_t *cmcfg = NULL;
	int result = -1;

	if ((cm == NULL) || (file == NULL)) {
		return -1;
	}

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

	cmcfg = (container_manager_config_t*)malloc(sizeof(container_manager_config_t));
	if (cmcfg == NULL) {
		result = -3;
		goto err_ret;
	}
	(void) memset(cmcfg, 0, sizeof(container_manager_config_t));
	dl_list_init(&cmcfg->role_list);
	dl_list_init(&cmcfg->bridgelist);
	dl_list_init(&cmcfg->operation.mount.mount_list);

	// Get configdir
	{
		const cJSON *configdir = NULL;
		configdir = cJSON_GetObjectItemCaseSensitive(json, "configdir");
		if (cJSON_IsString(configdir) && (configdir->valuestring != NULL)) {
			cmcfg->configdir = strdup(configdir->valuestring);
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"\ncmcfg: configdir %s\n", cmcfg->configdir);
			#endif
		} else {
			result = -2;
			goto err_ret;
		}
	}

	// Get bridge data
	{
		const cJSON *etherbridge = NULL;
		etherbridge = cJSON_GetObjectItemCaseSensitive(json, "etherbridge");
		if (cJSON_IsArray(etherbridge)) {
			cJSON *elem = NULL;

			cJSON_ArrayForEach(elem, etherbridge) {
				cJSON *name = NULL;
				container_manager_bridge_config_t *p = NULL;

				name = cJSON_GetObjectItemCaseSensitive(elem, "name");
				if (cJSON_IsString(name) && (name->valuestring != NULL)) {
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"cmcfg: etherbridge name = %s\n",name->valuestring);
					#endif
					;
				} else {
					// Mandatory value, drop this entry.
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"cmcfg: etherbridge name is from not set. It's mandatory value\n");
					#endif
					continue;
				}

				p = (container_manager_bridge_config_t*)malloc(sizeof(container_manager_bridge_config_t));
				if (p != NULL) {
					(void) memset(p, 0 , sizeof(container_manager_bridge_config_t));
					dl_list_init(&p->list);

					p->name = strdup(name->valuestring);
					dl_list_add_tail(&cmcfg->bridgelist, &p->list);
				}
			}
		}
	}

	// Get a operations
	{
		const cJSON *operation = NULL;
		operation = cJSON_GetObjectItemCaseSensitive(json, "operation");
		if (cJSON_IsObject(operation)) {
			cJSON *mount = NULL;

			mount = cJSON_GetObjectItemCaseSensitive(operation, "mount");
			if (cJSON_IsArray(mount)) {
				int ret = -1;

				ret = cmparser_manager_parse_operation_mount(&cmcfg->operation.mount, mount);
				if (ret < 0) {
					result = -2;
					goto err_ret;
				}
			}
		}
	}

	cJSON_Delete(json);
	cmparser_release_jsonstring(jsonstring);

	(*cm) = cmcfg;

	return 0;

err_ret:
	if (json != NULL) {
		cJSON_Delete(json);
	}

	if (jsonstring != NULL) {
		cmparser_release_jsonstring(jsonstring);
	}

	cmparser_manager_release_config(cmcfg);

	return result;
}
/**
 * Release container config allocated by cmparser_create_from_file
 *
 * @param [in]	cm	Pointer to container_manager_config_t.
 * @return void
 */
void cmparser_manager_release_config(container_manager_config_t *cm)
{

	if (cm == NULL) {
		return;
	}
	// operation config
	{
		container_manager_operation_mount_t *cmom = NULL;
		container_manager_operation_mount_elem_t *elem = NULL;

		cmom = &cm->operation.mount;
		while(dl_list_empty(&cmom->mount_list) == 0) {
			elem = dl_list_last(&cmom->mount_list, container_manager_operation_mount_elem_t, list);
			dl_list_del(&elem->list);
			(void) free(elem->to);
			(void) free(elem->filesystem);
			(void) free(elem->option);
			(void) free(elem->blockdev[0]);
			(void) free(elem->blockdev[1]);
			(void) free(elem);
		}
	}

	// base config
	{
		container_manager_bridge_config_t *elem = NULL;

		while(dl_list_empty(&cm->bridgelist) == 0) {
			elem = dl_list_last(&cm->bridgelist, container_manager_bridge_config_t, list);
			dl_list_del(&elem->list);
			(void) free(elem->name);
			(void) free(elem);
		}
	}

	// global
	(void) free(cm->configdir);
	(void) free(cm);
}
