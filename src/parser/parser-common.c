/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	parser.c
 * @brief	config file parser using cjson
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser/parser-common.h"

//#undef _PRINTF_DEBUG_
/**
 * Read json string with memory alocation.
 * Shall free string memory using cmparser_release_jsonstring.
 *
 * @param [in]	file	Full file path for json file
 * @return char*
 * @retval != NULL	Json string.
 * @retval == NULL	Error in file read
 */
char *cmparser_read_jsonstring(const char *file)
{
	int fd = -1;
	int ret = -1;
	ssize_t rsize = -1;
	char *strbuf = NULL;
	struct stat sb;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		return NULL;

	memset(&sb, 0, sizeof(sb));
	ret = fstat(fd, &sb);
	if (ret < 0)
		goto err_ret;

	if ((sb.st_mode & S_IFMT) != S_IFREG)
		goto err_ret;

	strbuf = (char*)malloc(sb.st_size + 1);
	if (strbuf == NULL)
		goto err_ret;

	memset(strbuf, 0, sb.st_size + 1);
	rsize = read(fd, strbuf, (sb.st_size));
	if (rsize < 0)
		goto err_ret;

	close(fd);

	return strbuf;

err_ret:
	if (strbuf != NULL)
		free(strbuf);
	
	if(fd >= 0)
		close(fd);

	return NULL;
}
/**
 * Release json string allocated by cmparser_read_jsonstring.
 *
 * @param [in]	jsonstring		Json string allocated by cmparser_read_jsonstring. 
 */
void cmparser_release_jsonstring(char *jsonstring)
{
	if (jsonstring != NULL)
		free(jsonstring);
}