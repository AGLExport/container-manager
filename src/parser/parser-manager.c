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
