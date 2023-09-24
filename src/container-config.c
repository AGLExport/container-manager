/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-config.c
 * @brief	This file include implementation for create and release top level container configs.
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
#include "container-workqueue.h"

#undef _PRINTF_DEBUG_

/**
 * @var		DEFAULT_CONF_PATH
 * @brief	Default container manager config path.
 */
static const char DEFAULT_CONF_PATH[] = "/etc/container-manager.json";

/**
 * Bind all guest container to role list.
 * Container manager launch one guest container per one role.
 * The role list manage which container is active, which container is inactive.
 * This function create role list from all guest container config data.
 *
 * @param [in]	cs	Pointer to constructed containers_t data.
 * @return int
 * @retval 0	Success to role list creation.
 * @retval -1	Fail to role list creation. (Reserve)
 */
static int bind_container_to_role_list(containers_t* cs)
{
    int ret = 0;

	// create role list and set guest link.
	for(int i=0; i< cs->num_of_container; i++) {
		container_config_t *cc = NULL;
		char *role = NULL;
		int is_set = 0;

		cc = cs->containers[i];
		role = cc->role;
		{
			container_manager_role_config_t *cmrc = NULL;

			dl_list_for_each(cmrc, &cs->cmcfg->role_list, container_manager_role_config_t, list) {
				if (cmrc->name != NULL) {
					ret = strcmp(cmrc->name, role);
					if (ret == 0) {
						// add guest info to existing role
						container_manager_role_elem_t *pelem = NULL;

						#ifdef _PRINTF_DEBUG_
						(void) fprintf(stdout,"bind_container_to_role: add %s to existing role %s\n", cc->name, cmrc->name);
						#endif

						pelem = (container_manager_role_elem_t*)malloc(sizeof(container_manager_role_elem_t));
						if (pelem != NULL) {

							(void) memset(pelem, 0 , sizeof(container_manager_role_elem_t));
							dl_list_init(&pelem->list);
							pelem->cc = cc;	//set guest info

							if (cc->baseconfig.autoboot == 1) {
								// add top
								dl_list_add(&cmrc->container_list, &pelem->list);
							} else {
								// add tail
								dl_list_add_tail(&cmrc->container_list, &pelem->list);
							}
						} else {
							;	//skip data creation
						}

						is_set = 1;
					}
				}
			}
		}

		if (is_set == 0) {
			// create new role
			container_manager_role_config_t *cmrc = NULL;

			cmrc = (container_manager_role_config_t*)malloc(sizeof(container_manager_role_config_t));
			if (cmrc != NULL) {
				container_manager_role_elem_t *pelem = NULL;

				(void) memset(cmrc, 0 , sizeof(container_manager_role_config_t));
				dl_list_init(&cmrc->list);
				dl_list_init(&cmrc->container_list);

				cmrc->name = strdup(role);
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"cmcfg: create new role %s\n", cmrc->name);
				#endif

				// create terminator
				pelem = (container_manager_role_elem_t*)malloc(sizeof(container_manager_role_elem_t));
				if (pelem != NULL) {

					(void) memset(pelem, 0 , sizeof(container_manager_role_elem_t));
					dl_list_init(&pelem->list);
					pelem->cc = NULL;	//dummy guest info

					dl_list_add_tail(&cmrc->container_list, &pelem->list);
					dl_list_add_tail(&cs->cmcfg->role_list, &cmrc->list);
				} else {
					(void) free(cmrc->name);
					(void) free(cmrc);
					continue;	//skip data creation
				}

				// add guest info to new role
				pelem = (container_manager_role_elem_t*)malloc(sizeof(container_manager_role_elem_t));
				if (pelem != NULL) {

					(void) memset(pelem, 0 , sizeof(container_manager_role_elem_t));
					dl_list_init(&pelem->list);
					pelem->cc = cc;	//set guest info

					if (pelem->cc->baseconfig.autoboot == 1) {
						// add top
						dl_list_add(&cmrc->container_list, &pelem->list);
					} else {
						// add tail
						dl_list_add_tail(&cmrc->container_list, &pelem->list);
					}
				} else {
					;	//skip data creation
				}
			} else {
				;	//skip data creation
			}

		}

	}

	return 0;
}
/**
 * Role list cleanup.
 *
 * @param [in]	cs	Pointer to constructed containers_t data.
 * @return int
 * @retval 0	Success to role list cleanup with memory free.
 * @retval -1	Fail to role list cleanup.
 */
static int role_list_cleanup(containers_t* cs)
{
    int ret = 0;

	if (cs == NULL) {
		return -1;
	}

	if (cs->cmcfg == NULL) {
		return -1;
	}

	while(dl_list_empty(&cs->cmcfg->role_list) == 0) {
		container_manager_role_config_t *cmrc = NULL;

		cmrc = dl_list_last(&cs->cmcfg->role_list, container_manager_role_config_t, list);

		while(dl_list_empty(&cmrc->container_list) == 0) {
			container_manager_role_elem_t *pelem = NULL;
			pelem = dl_list_last(&cmrc->container_list, container_manager_role_elem_t, list);
			dl_list_del(&pelem->list);
			(void) free(pelem);
		}

		dl_list_del(&cmrc->list);
		(void) free(cmrc->name);
		(void) free(cmrc);
	}

	return ret;
}
/**
 * qsort compare function for container boot pri. sorting
 *
 * @param [in]	data1	Pointer to data no 1.
 * @param [in]	data2	Pointer to data no 2.
 * @return int
 * @retval 0 same boot pri between data1 and data2.
 * @retval -1 data1 is higher than data2.
 * @retval 1 data2 is higher than data1.
 */
static int compare_bootpri(const void *data1, const void *data2)
{
    int ret = 0;
	const container_config_t *cc1 = *(const container_config_t**)data1;
	const container_config_t *cc2 = *(const container_config_t**)data2;
	int pri1 = 0, pri2 = 0;

	pri1 = cc1->baseconfig.bootpriority;
	pri2 = cc2->baseconfig.bootpriority;

	if (pri1 < pri2) {
		ret = -1;
	} else if (pri1 > pri2) {
		ret = 1;
	} else {
		ret = 0;
	}

	return ret;
}

/**
 * Scan and create container configuration data
 *
 * @param [in]	config_file	File path for container manager configs.
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
	size_t slen = 0, buflen = 0;

	(void) memset(ca,0,sizeof(ca));

	conffile = config_file;
	if (conffile == NULL) {
		conffile = DEFAULT_CONF_PATH;
	}

	ret = cmparser_manager_create_from_file(&cm, conffile);
	if (ret < 0) {
		return NULL;
	}

	confdir = cm->configdir;

	(void) memset(buf,0,sizeof(buf));
	buflen = sizeof(buf) - 1u;
	(void) strncpy(buf, confdir, buflen);
	slen = strlen(buf);
	if (slen <= 0u) {
		return NULL;
	}

	if (buf[slen-1u] != '/') {
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
				if (!(num < GUEST_CONTAINER_LIMIT)) {
					#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
					(void) fprintf(stderr,"[CM CRITICAL ERROR] Number of guest containers was over to limit.");
					#endif
					break;
				}

				if (strstr(dent->d_name, ".json") != NULL) {

					buf[slen] = '\0';
					buf[(sizeof(buf) - 1u)] = '\0';
					(void) strncpy(&buf[slen], dent->d_name, buflen);

					// parse container config.
					ret = cmparser_create_from_file(&cc, buf);
					if (ret < 0) {
						#ifdef _PRINTF_DEBUG_
						(void) fprintf(stdout, "[FAIL] cmparser_create_from_file %s ret = %d\n", buf, ret);
						#endif
						continue;
					}
					(void)container_workqueue_initialize(&(cc->workqueue));
					cc->runtime_stat.status = CONTAINER_DISABLE;
					ca[num] = cc;
					num = num + 1;
				}
			}
		}
		while(dent != NULL);

		(void) closedir(dir);
	}

	if (num <= 0) {
		#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
		(void) fprintf(stderr,"[CM CRITICAL ERROR] Did not find guest container config at %s.\n", confdir);
		#endif
		goto err_ret;
	}

	// sort boot pri
	qsort(ca, num, sizeof(container_config_t*),compare_bootpri);

	//Create containers_t data
	cs = (containers_t*)malloc(sizeof(containers_t));
	if (cs == NULL) {
		goto err_ret;
	}

	(void) memset(cs, 0, sizeof(containers_t));

	cs->containers = (container_config_t**)malloc(sizeof(container_config_t*) * (size_t)num);
	if (cs->containers == NULL) {
		goto err_ret;
	}

	(void) memset(cs->containers, 0, sizeof(container_config_t*) * (size_t)num);

	for(int i=0; i < num; i++) {
		cs->containers[i] = ca[i];
	}
	cs->num_of_container = num;

	cs->cmcfg = cm;
	ret = bind_container_to_role_list(cs);
	if(ret < 0) {
		goto err_ret;
	}

	return cs;

err_ret:
	(void) role_list_cleanup(cs);

	for(int i=0; i < num;i++) {
		(void)container_workqueue_deinitialize(&(ca[i]->workqueue));
		cmparser_release_config(ca[i]);
	}

	if (cs !=NULL) {
		(void) free(cs->containers);
	}

	(void) free(cs);

	if (cm != NULL) {
		cmparser_manager_release_config(cm);
	}

	return NULL;
}
/**
 * Release container configs with sub sections cleanup.
 *
 * @param [in]	cs	Pointer to constructed containers_t data.
 * @return int
 * @retval 0	Success to container configs cleanup with memory free.
 * @retval -1	Fail to container configs cleanup.
 */
int release_container_configs(containers_t *cs)
{
	int num = 0;

	if (cs == NULL) {
		return -1;
	}

	(void) role_list_cleanup(cs);

	num = cs->num_of_container;

	for(int i=0; i<num;i++) {
		int ret = -1;
		ret = container_workqueue_deinitialize(&(cs->containers[i]->workqueue));
		if (ret != -2) {
			cmparser_release_config(cs->containers[i]);
		} else {
			//For crash safe, some resources shall leak.
			;	//nop
		}
	}

	(void) free(cs->containers);
	cmparser_manager_release_config(cs->cmcfg);
	(void) free(cs);

	return 0;
}
