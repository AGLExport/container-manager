/**
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file	libsocketcangw_config.c
 * @brief	config file parser using cjson
 */
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#include <libsocketcangw.h>
#include <libsocketcangw_config.h>

const size_t g_cangw_config_max_bytes = (1024U * 1024U);
const int g_cangw_filter_max = 64;

static int g_log_output_enable = 0;
/**
 * Write a printf-style log message to stderr without changing errno.
 * The caller supplies any trailing newline.
 *
 * @return Number of characters written, or a negative value on output failure.
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
static void cangw_config_log(const char *format, ...)
{
	if (g_log_output_enable == 1) {
		va_list args;

		va_start(args, format);
		(void) vfprintf(stderr, format, args);
		va_end(args);
	}
}

/**
 * Enable log output.
 *
 * @return Previous log output enable status.
 */
int cangw_config_log_enable(void)
{
	int prev = g_log_output_enable;
	g_log_output_enable = 1;
	return prev;
}

/**
 * Read json string with memory allocation.
 * Shall free string memory using cangw_config_release_jsonstring.
 *
 * @param [in]	file	Full file path for json file
 * @return char*
 * @retval != NULL	Json string.
 * @retval == NULL	Error in file read
 */
static char *cangw_config_read_jsonstring(const char *file)
{
	int fd = -1;
	int ret = -1;
	ssize_t rsize = -1;
	char *strbuf = NULL, *tmpstrbuf = NULL;
	struct stat sb;
	size_t strbuf_size = 0;

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		cangw_config_log("cangw: Failed to open file: %s\n", file);
		goto err_ret;
	}

	(void) memset(&sb, 0, sizeof(sb));
	ret = fstat(fd, &sb);
	if (ret < 0) {
		cangw_config_log("cangw: Failed to get file status: %s\n", file);
		goto err_ret;
	}

	if ((sb.st_mode & S_IFMT) != S_IFREG) {
		cangw_config_log("cangw: File is not a regular file: %s\n", file);
		goto err_ret;
	}

	if (sb.st_size > g_cangw_config_max_bytes) {
		cangw_config_log("cangw: File size exceeds limit: %s\n", file);
		goto err_ret;
	}

	strbuf = (char*)malloc(sb.st_size + 1);
	if (strbuf == NULL) {
		cangw_config_log("cangw: Failed to allocate memory for file content: %s\n", file);
		goto err_ret;
	}
	(void) memset(strbuf, 0, sb.st_size + 1);

	strbuf_size = sb.st_size;
	tmpstrbuf = strbuf;

	do {
		rsize = read(fd, tmpstrbuf, strbuf_size);
		if (rsize < 0) {
			if (errno == EINTR) {
				continue;
			}
			goto err_ret;
		} else if (rsize == 0) {
			break;
		} else if (rsize > 0) {
			tmpstrbuf += rsize;
			strbuf_size -= (size_t)rsize;
		}
	} while (1);

	(void) close(fd);

	return strbuf;

err_ret:
	(void) free(strbuf);

	if(fd >= 0) {
		(void) close(fd);
	}

	return NULL;
}
/**
 * Release json string allocated by cangw_config_read_jsonstring.
 *
 * @param [in]	jsonstring		Json string allocated by cangw_config_read_jsonstring.
 */
static void cangw_config_release_jsonstring(char *jsonstring)
{
	(void) free(jsonstring);
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
 * @retval -4 Unsupported version.
 */
static int get_hex_value_u32(uint32_t *value, const char *str)
{
	int result = -1;
	char *endptr = NULL;
	unsigned long int val = 0;

	if ((str == NULL) || (value == NULL)) {
		goto do_return;
	}

	if (strlen(str) < 3) {
		goto do_return;
	}

	if (str[0] != '0' || (str[1] != 'x' && str[1] != 'X')) {
		goto do_return;
	}

	errno = 0;
	val = strtoul(str, &endptr, 16);
	if ((errno != 0) || (endptr == NULL) || (endptr == str) || (*endptr != '\0')) {
		goto do_return;
	}

	if (val > UINT32_MAX) {
		goto do_return;
	}

	*value = (uint32_t)val;

	result = 0;

do_return:
	return result;
}
/**
 * Create socketcan gateway rule object from json filter.
 *
 * @param [out]	pprule		Double pointer to socketcan_gw_rule_t to out socketcan_gw_rule_t object.
 * @param [in]	filter		Pointer to cJSON object representing the filter.
 * @return int
 * @retval  0 Success create socketcan_gw_rule_t object.
 * @retval -1 Argument error.
 * @retval -2 Filter value error.
 * @retval -3 Memory allocation error.
 */
static int cangw_parse_filter(socketcan_gw_rule_t** pprule, const cJSON *filter)
{
	int result = -1;
	cJSON *type = NULL;
	socketcan_gw_rule_t *prule;
	socketcan_gw_rule_t rule;

	if ((pprule == NULL) || (filter == NULL)) {
		goto do_return;
	}

	memset(&rule, 0, sizeof(socketcan_gw_rule_t));

	type = cJSON_GetObjectItemCaseSensitive(filter, "can_id");
	if (cJSON_IsString(type) && (type->valuestring != NULL)) {
		uint32_t id = 0;
		int ret = get_hex_value_u32(&id, type->valuestring);
		if (ret != 0) {
			result = -2;
			cangw_config_log("cangw: Invalid can_id value in filter: %s\n", type->valuestring);
			goto do_return;
		}

		if ((id & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG)) != 0) {
			// the value of can_id must set 0 to flags field.
			result = -2;
			cangw_config_log("cangw: Invalid can_id value in filter: %s\n", type->valuestring);
			goto do_return;
		}
		rule.filter.can_id = id;
	} else {
		// can_id is mandatory
		result = -1;
		cangw_config_log("cangw: No can_id in filter.\n");
		goto do_return;
	}

	type = cJSON_GetObjectItemCaseSensitive(filter, "can_mask");
	if (cJSON_IsString(type) && (type->valuestring != NULL)) {
		uint32_t mask = 0;
		int ret = get_hex_value_u32(&mask, type->valuestring);
		if (ret != 0) {
			result = -2;
			cangw_config_log("cangw: Invalid can_mask value in filter: %s\n", type->valuestring);
			goto do_return;
		}

		if ((mask & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG)) != 0) {
			// the value of can_mask must set 0 to flags field.
			result = -2;
			cangw_config_log("cangw: Invalid can_mask value in filter: %s\n", type->valuestring);
			goto do_return;
		}
		rule.filter.can_mask = mask;
	} else {
		// can_mask is mandatory
		result = -1;
		cangw_config_log("cangw: No can_mask in filter.\n");
		goto do_return;
	}

	type = cJSON_GetObjectItemCaseSensitive(filter, "invert");
	if (type != NULL) {
		if (cJSON_IsTrue(type)) {
			rule.invert_can_id = 1;
		} else {
			rule.invert_can_id = 0;
		}
	} else {
		// If ite does not have invert key, this value set false.
		rule.invert_can_id = 0;
		cangw_config_log("cangw: No invert entry in filter. Use default (false).\n");
	}

	type = cJSON_GetObjectItemCaseSensitive(filter, "echo");
	if (type != NULL) {
		if (cJSON_IsTrue(type)) {
			rule.echo = 1;
		} else {
			// If echo is not set, this value set zero.
			rule.echo = 0;
		}
	} else {
		// If ite does not have echo key, this value set zero.
		rule.echo = 0;
		cangw_config_log("cangw: No echo entry in filter. Use default (false).\n");
	}

	type = cJSON_GetObjectItemCaseSensitive(filter, "direction");
	if (cJSON_IsString(type) && type->valuestring != NULL) {
		if (strcmp(type->valuestring, "out") == 0) {
			rule.direction = SOCKETCAN_GW_DIRECTION_OUT;
		} else if (strcmp(type->valuestring, "in") == 0) {
			rule.direction = SOCKETCAN_GW_DIRECTION_IN;
		} else {
			rule.direction = SOCKETCAN_GW_DIRECTION_IN;
			cangw_config_log("cangw: Invalid direction value in filter: %s\n", type->valuestring);
		}
	} else {
		rule.direction = SOCKETCAN_GW_DIRECTION_IN;
		cangw_config_log("cangw: Use default direction (in).\n");
	}

	prule = (socketcan_gw_rule_t*)malloc(sizeof(socketcan_gw_rule_t));
	if (prule == NULL) {
		result = -3;
		cangw_config_log("cangw: Failed to allocate memory in cangw_parse_filter\n");
		goto do_return;
	}

	memset(prule, 0, sizeof(socketcan_gw_rule_t));

	prule->filter.can_id  = rule.filter.can_id;
	prule->filter.can_mask = rule.filter.can_mask;
	prule->echo = rule.echo;
	prule->invert_can_id = rule.invert_can_id;
	prule->direction = rule.direction;

	(*pprule) = prule;
	result = 0;

do_return:
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
 * @retval -4 Unsupported version.
 */
static int cangw_parse_filters_v1(socketcan_gw_rule_t ***ppprules, size_t *parray_num, const cJSON *filters)
{
	int result = -1;
	socketcan_gw_rule_t **pprules = NULL;
	int json_array_size = 0;
	size_t array_num = 0;

	if ((ppprules == NULL) || (parray_num == NULL) || (filters == NULL)) {
		result = -1;
		goto err_ret;
	}

	if (cJSON_IsArray(filters)) {
		cJSON *elem = NULL;

		json_array_size = cJSON_GetArraySize(filters);
		if (g_cangw_filter_max < json_array_size) {
			result = -2;
			cangw_config_log("cangw: Array size exceeds the limit in format version 1.\n");
			goto err_ret;
		} else if (json_array_size <= 0) {
			result = -2;
			cangw_config_log("cangw: Invalid array size in format version 1.\n");
			goto err_ret;
		}

		pprules = (socketcan_gw_rule_t**)malloc(sizeof(socketcan_gw_rule_t*) * (size_t)json_array_size);
		if (pprules == NULL) {
			result = -3;
			cangw_config_log("cangw: Failed to allocate memory in format version 1.\n");
			goto err_ret;
		}
		(void) memset(pprules, 0, sizeof(socketcan_gw_rule_t*) * (size_t)json_array_size);

		cJSON_ArrayForEach(elem, filters) {
			if (cJSON_IsObject(elem)) {
				int ret = -1;

				ret = cangw_parse_filter(&pprules[array_num], elem);
				if (ret < 0) {
					if (ret == -3) {
						result = -3;
					} else {
						result = -2;
						cangw_config_log("cangw: Failed to parse filter in format version 1.\n");
					}
					goto err_ret;
				}
				array_num++;
			} else {
				result = -2;
				cangw_config_log("cangw: Invalid filter format in format version 1.\n");
				goto err_ret;
			}
		}
	} else {
		result = -2;
		cangw_config_log("cangw: The filter does not array in format version 1.\n");
		goto err_ret;
	}

	(*ppprules) = pprules;
	(*parray_num) = array_num;

	return 0;

err_ret:
	if (pprules != NULL) {
		for (size_t i = 0; i < array_num; i++) {
			free(pprules[i]);
		}

		(void) free(pprules);
	}

	return result;
}

/**
 * Create socketcan gateway rules object from json file.
 *
 * @param [out]	ppsgr	Double pointer to socketcan_gw_rules_t to out socketcan_gw_rules_t object.
 * @param [in]	file	Full file path for json file
 * @return int
 * @retval  0 Success create socketcan_gw_rules_t object.
 * @retval -1 Argument error.
 * @retval -2 Json file parse error.
 * @retval -3 Memory allocation error.
 * @retval -4 Unsupported version.
 */
int cangw_config_load_from_file(socketcan_gw_rules_t **ppsgr, const char *file)
{
	cJSON *json = NULL;
	char *jsonstring = NULL;
	socketcan_gw_rules_t *psgr = NULL;
	int result = -1;

	if ((ppsgr == NULL) || (file == NULL)) {
		cangw_config_log("cangw: Invalid arguments in cangw_config_load_from_file.\n");
		return -1;
	}

	jsonstring = cangw_config_read_jsonstring(file);
	if (jsonstring == NULL) {
		result = -2;
		goto err_ret;
	}

	json = cJSON_ParseWithOpts(jsonstring, NULL, cJSON_False);
	if (json == NULL) {
		result = -2;
		cangw_config_log("cangw: Failed to parse JSON.\n");
		goto err_ret;
	}

	psgr = (socketcan_gw_rules_t*)malloc(sizeof(socketcan_gw_rules_t));
	if (psgr == NULL) {
		result = -3;
		cangw_config_log("cangw: Failed to allocate memory in cangw_config_load_from_file.\n");
		goto err_ret;
	}
	(void) memset(psgr, 0, sizeof(socketcan_gw_rules_t));

	// version check
	{
		const cJSON *version = NULL;
		int version_num = 0;
		version = cJSON_GetObjectItemCaseSensitive(json, "version");
		if (cJSON_IsNumber(version)) {
			version_num = version->valueint;
		}

		if (version_num == 1) {
			cJSON *filters = cJSON_GetObjectItemCaseSensitive(json, "filters");
			if (filters != NULL) {
				int ret = cangw_parse_filters_v1(&(psgr->rules), &(psgr->rule_num), filters);
				if (ret < 0) {
					if (ret == -3) {
						result = -3;
					} else {
						result = -2;
					}
					goto err_ret;
				}
				psgr->array_num = psgr->rule_num;
			} else {
				result = -2;
				cangw_config_log("cangw: The 'filters' array is missing in format version 1.\n");
				goto err_ret;
			}
		} else {
			result = -4;
			cangw_config_log("cangw: Unsupported format version %d.\n", version_num);
			goto err_ret;
		}
	}

	cJSON_Delete(json);
	cangw_config_release_jsonstring(jsonstring);

	(*ppsgr) = psgr;

	return 0;

err_ret:
	(void) cangw_config_log("cangw: Failed to load gateway configuration '%s' (error %d)\n", file, result);
	if (json != NULL) {
		cJSON_Delete(json);
	}

	if (jsonstring != NULL) {
		cangw_config_release_jsonstring(jsonstring);
	}

	cangw_config_release_config(psgr);

	return result;
}
/**
 * Release container config allocated by cangw_load_config_from_file
 *
 * @param [in]	psgr	Pointer to socketcan_gw_rules_t.
 * @return void
 */
void cangw_config_release_config(socketcan_gw_rules_t *psgr)
{

	if (psgr == NULL) {
		return;
	}

	for (size_t i = 0; i < psgr->rule_num; i++) {
		if (psgr->rules[i] != NULL) {
			(void) free(psgr->rules[i]);
		}
	}

	free(psgr->rules);
	free(psgr);
}

/**
 * Fill ifindex values for all rules in the gateway configuration.
 *
 * @param [in,out]	psgr	Pointer to socketcan_gw_rules_t.
 * @param [in]	dst_ifindex	Destination interface index.
 * @param [in]	src_ifindex	Source interface index.
 * @return int
 * @retval  0 Success.
 * @retval -1 Invalid arguments.
 */
int cangw_config_refill_ifindex(socketcan_gw_rules_t *psgr, unsigned int dst_ifindex, unsigned int src_ifindex)
{
	if (psgr == NULL) {
		cangw_config_log("cangw: Invalid arguments in cangw_config_refill_ifindex.\n");
		return -1;
	}

	for (size_t i = 0; i < psgr->rule_num; i++) {
		if (psgr->rules[i] != NULL) {
			psgr->rules[i]->dst_ifindex = dst_ifindex;
			psgr->rules[i]->src_ifindex = src_ifindex;
		}
	}

	return 0;
}

#ifdef LIBSOCKETCANGW_CONFIG_CHECKER
size_t json_check_duplicate_keys(const cJSON *node, FILE *errors)
{
	const cJSON *item;
	size_t duplicates = 0;

	if (!cJSON_IsObject(node) && !cJSON_IsArray(node))
		return 0;

	cJSON_ArrayForEach(item, node) {
		if (cJSON_IsObject(node)) {
			const cJSON *previous;

			for (previous = node->child; previous != item;
				previous = previous->next) {
				if (strcmp(previous->string, item->string) == 0) {
					if (errors != NULL) {
						fprintf(errors,
							"duplicate JSON key: \"%s\"\n",
							item->string);
					}
					duplicates++;
					break;
				}
			}
		}

		duplicates += json_check_duplicate_keys(item, errors);
	}

	return duplicates;
}
#endif //#ifdef LIBSOCKETCANGW_CONFIG_CHECKER