/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cluster-service.c
 * @brief	main source file for cluster-service
 */
#include "parser/parser-manager.h"
#include "parser/parser-container.h"
#include "lxc-util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include "container-config.h"

static const char DEFAULT_CONF_PATH[] = "/etc/container-manager.json";
#define GUEST_CONTAINER_LIMIT	(8)	/** < Limit value for container name */

/**
 * qsort compare function for container boot pri. sorting
 *
 * @param [in]	data1	data 1
 * @param [in]	data1	data 2
 * @return int
 * @retval 0 same boot pri between data1 and data2.
 * @retval -1 data1 is higher than data2.
 * @retval 1 data2 is higher than data1.
 */
static int compare_bootpri(const void *data1, const void *data2)
{
    int ret = 0;
	const container_config_t *cc1 = *(container_config_t**)data1;
	const container_config_t *cc2 = *(container_config_t**)data2;
	int pri1 = 0, pri2 = 0;

	pri1 = cc1->baseconfig.bootpriority;
	pri2 = cc2->baseconfig.bootpriority;

	if (pri1 < pri2)
		ret = -1;
	else if (pri1 > pri2)
		ret = 1;

	return ret;
}

/**
 * Scan and create container configuration data
 *
 * @param [in]	config_dir	Scan dir for container configs.
 * @return containers_t*
 * @retval NULL Fail to create config.
 * @retval != NULL Available containers_t* data.
 */
containers_t *create_container_configs(const char *config_file)
{
	int num = 0;
	int ret = -1;
	containers_t *cs = NULL;
	container_manager_config_t *cm = NULL;
	container_config_t *ca[GUEST_CONTAINER_LIMIT];
	container_config_t *cc = NULL;
	DIR *dir = NULL;
	const char *confdir = NULL;
	const char *conffile = NULL;
	char buf[1024];
	int slen = 0, buflen = 0;

	memset(ca,0,sizeof(ca));

	conffile = config_file;
	if (conffile == NULL) {
		conffile = DEFAULT_CONF_PATH;
	}

	ret = cmparser_manager_create_from_file(&cm, conffile);
	if (ret < 0)
		return NULL;

	confdir = cm->configdir;

	memset(buf,0,sizeof(buf));
	buflen = sizeof(buf) - 1;
	strncpy(buf, confdir, buflen);
	slen = strlen(buf);
	if (slen <= 0)
		return NULL;

	if (buf[slen-1] != '/') {
		buf[slen] = '/';
		slen++;
	}
	buflen = buflen - slen;

	// Scan config dir and pick up config file
	dir = opendir(confdir);	//close-on-exec set default
	if (dir != NULL) {
		struct dirent *dent = NULL;

		do {
			dent = readdir(dir);
			if (dent != NULL) {
				if (strstr(dent->d_name, ".json") != NULL) {

					buf[slen] = '\0';
					(void)strncat(buf, dent->d_name, buflen);

					// parse container config.
					ret = cmparser_create_from_file(&cc, buf);
					if (ret < 0) {
						#ifdef _PRINTF_DEBUG_
						fprintf(stderr, "[FAIL} cmparser_create_from_file %s ret = %d\n", buf, ret);
						#endif
						continue;
					}
					ca[num] = cc;
					num = num + 1;
					if (num >= GUEST_CONTAINER_LIMIT)
						break;
				}
			}
		}
		while(dent != NULL);

		closedir(dir);
	}

	if (num <= 0) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "[FAIL} No container config at %s\n", confdir);
		#endif
		goto err_ret;
	}

	// sort boot pri
	qsort(ca, num, sizeof(container_config_t*),compare_bootpri);

	//Create containers_t data
	cs = (containers_t*)malloc(sizeof(containers_t));
	if (cs == NULL)
		goto err_ret;

	memset(cs, 0, sizeof(containers_t));

	cs->containers = (container_config_t**)malloc(sizeof(container_config_t*)*num);
	if (cs->containers == NULL)
		goto err_ret;

	memset(cs->containers, 0, sizeof(container_config_t*)*num);

	for(int i=0; i < num; i++) {
		cs->containers[i] = ca[i];
	}
	cs->num_of_container = num;

	cs->cmcfg = cm;

	return cs;

err_ret:
	for(int i=0; i < num;i++) {
		cmparser_release_config(ca[i]);
	}

	if (cs !=NULL)
		free(cs->containers);

	free(cs);

	if (cm != NULL)
		cmparser_manager_release_config(cm);

	return NULL;
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
int release_container_configs(containers_t *cs)
{
	int num = 0;

	if (cs == NULL)
		return -1;

	num = cs->num_of_container;

	for(int i=0; i<num;i++) {
		cmparser_release_config(cs->containers[i]);
	}

	free(cs->containers);
	cmparser_manager_release_config(cs->cmcfg);
	free(cs);

	return 0;
}
