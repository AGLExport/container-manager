/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	proc-util.c
 * @brief	A procfs utility functions
 */
#include "proc-util.h"

#include <stddef.h>
#include <asm/setup.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"

#undef _PRINTF_DEBUG_

#ifndef COMMAND_LINE_SIZE
//Workaround for x86-64
#define COMMAND_LINE_SIZE	(2048)
#endif	//#ifndef COMMAND_LINE_SIZE
/**
 * @struct	s_procutl_cmdline_elem
 * @brief	The data structure for cmdline argument list.
 */
struct s_procutl_cmdline_elem {
	struct dl_list list;	/**< Double link list header. */
	char*	var;
	char*	value;
};
typedef struct s_procutl_cmdline_elem procutl_cmdline_elem_t;	/**< typedef for struct s_procutl_cmdline_elem. */
/**
 * @struct	s_procutlr
 * @brief	The data structure for procutil.
 */
struct s_procutil {
	char *org_buf_ptr;				/**< Original cmdline buffer pointer to use memory free. */
	struct dl_list argument_list;	/**< Double link list for s_procutl_cmdline_elem */
};
typedef struct s_procutil procutil_t;	/**< typedef for struct s_procutl. */

/**
 * Get kernel cmdline from /proc/cmdline.
 *
 * @param [out]	ppbuf		Double pointer to char to get allocated memory buffer. Need to free caller.
 * @param [out]	ppbuf_size	Pointer to int to get allocated memory buffer size.
 * @return int
 * @retval  0 Success to get cmdline.
 * @retval -1 Internal error.
 */
static int procutil_get_cmdline(char **ppbuf, int *ppbuf_size)
{
	int fd = -1;
	int is_available = 0;
	int result = -1;
	ssize_t ret = -1;
	ssize_t buffer_size = 0, buffer_size_max = 0;
	char *pbuf = NULL;

	fd = open("/proc/cmdline", (O_RDONLY | O_CLOEXEC));
	if (fd < 0)
		return -1;

	// set initial buffer size and maximum buffer size.
	buffer_size = COMMAND_LINE_SIZE * 2;
	buffer_size_max = ((64*1024) > (COMMAND_LINE_SIZE*8)) ? (64*1024) : (COMMAND_LINE_SIZE*8);

	while (buffer_size <= buffer_size_max) {
		free(pbuf);
		pbuf = (char*)malloc(buffer_size);
		if (pbuf == NULL) {
			result = -1;
			goto error_ret;
		}

		(void) memset(pbuf, 0, buffer_size);

		do {
			ret = read(fd, pbuf, buffer_size);
		} while (ret == -1 && errno == EINTR);

		if (ret < buffer_size) {
			// success to read
			is_available = 1;
			(*ppbuf_size) = (int)buffer_size;
			(*ppbuf) = pbuf;
			#ifdef _PRINTF_DEBUG_
			fprintf(stdout,"procutil: Got cmdline %s\n", pbuf);
			#endif
			break;
		}

		// retry read
		buffer_size = buffer_size * 2;
	}

	close(fd);

	return 0;

error_ret:
	free(pbuf);

	if (fd != -1)
		close(fd);

	return result;
}
/**
 * Create cmdline argument list.
 * When this function return error, need to call procutil_cleanup.
 *
 * @param [in,out]	pu		Pointer to procutil_t.
 * @param [in]	buf_size	Size of cmdline buffer in pu.
 * @return int
 * @retval  0 Success to create cmdline argument list.
 * @retval -1 Internal error.
 */
static int procutil_create_argument_list(procutil_t *pu, int buf_size)
{
	int result = -1;
	int counter = 0;
	char *pbuf = NULL,*pbuf_last = NULL;

	pbuf = pu->org_buf_ptr;
	pbuf_last = pbuf;

	while(counter < buf_size) {
		if (pbuf[counter] == ' ' || pbuf[counter] == '\n' || pbuf[counter] == '\0') {
			int len = 0;
			int is_term = 0;

			if (pbuf[counter] == '\n' || pbuf[counter] == '\0')
				is_term = 1;

			// Got space
			pbuf[counter] = '\0';
			len = strlen(pbuf_last);
			if (len > 0) {
				procutl_cmdline_elem_t *pelem = NULL;

				pelem = (procutl_cmdline_elem_t*)malloc(sizeof(procutl_cmdline_elem_t));
				if (pelem == NULL) {
					result = -1;
					goto error_ret;
				}

				pelem->var = pbuf_last;
				pelem->value = NULL;
				dl_list_init(&pelem->list);

				for(int i=0; i < len; i++) {
					if (pbuf_last[i] == '=') {
						// Got 1st '='
						pbuf_last[i] = '\0';
						if ((i+1) < len) {
							// have value
							pelem->value = &pbuf_last[i+1];
						}
					}
				}
				dl_list_add_tail(&pu->argument_list, &pelem->list);
				#ifdef _PRINTF_DEBUG_
				fprintf(stdout,"procutil: Got cmdline arg %s = %s\n", pelem->var, pelem->value);
				#endif
			}

			if (is_term == 1)
				break;

			pbuf_last = &pbuf[counter+1];
		}

		counter++;
	}

	return 0;

error_ret:

	return result;
}
/**
 * Create procutil object.
 *
 * @param [out]	ppu	Double pointer to procutil_t to get procutil_t object.
 * @return int
 * @retval	0	Success to create procutil_t.
 * @retval	-1	Internal error.
 * @retval	-2	Argument error.
 */
int procutil_create(procutil_t **ppu)
{
	int ret = -1;
	int result = -1;
	char *pbuf = NULL;
	int pbuf_size = 0;
	procutil_t *pu = NULL;

	if (ppu == NULL)
		return -2;

	pu = (procutil_t*)malloc(sizeof(procutil_t));
	if (pu == NULL)
		return -1;

	(void) memset(pu, 0, sizeof(procutil_t));
	dl_list_init(&pu->argument_list);

	ret = procutil_get_cmdline(&pbuf, &pbuf_size);
	if (ret < 0) {
		result = -1;
		goto error_ret;
	}

	pu->org_buf_ptr = pbuf;
	ret = procutil_create_argument_list(pu, pbuf_size);
	if (ret < 0)
		goto error_ret;

	(*ppu) = pu;

	return 0;

error_ret:
	procutil_cleanup(pu);

	return result;
}
/**
 * Cleanup procutil object.
 *
 * @param [in]	pu	Pointer to get procutil_t object.
 * @return int
 * @retval	0	Success to cleanup procutil_t.
 * @retval	-1	Internal error.
 * @retval	-2	Argument error.
 */
int procutil_cleanup(procutil_t *pu)
{
	procutl_cmdline_elem_t *pelem = NULL;

	if (pu == NULL)
		return -1;

	while(dl_list_empty(&pu->argument_list) == 0) {
		pelem = dl_list_last(&pu->argument_list, procutl_cmdline_elem_t, list);
		dl_list_del(&pelem->list);
		free(pelem);
	}

	free(pu->org_buf_ptr);
	free(pu);

	return 0;
}
/**
 * Get int64 value from cmdline.
 *
 * @param [in]	pu		Pointer to get procutil_t object.
 * @param [in]	key		Scanning key.
 * @param [out]	value	Pointer to int64_t storage to get value.
 * @return int
 * @retval	0	Success get value.
 * @retval	-1	Convert error.
 * @retval	-2	Argument error.
 * @retval	-3	A key is not found.
 */
int procutil_get_cmdline_value_int64(procutil_t *pu, const char *key, int64_t *value)
{
	int result = -3;
	procutl_cmdline_elem_t *pelem = NULL;
	char *endptr = NULL;
	int64_t v = 0;

	if (pu == NULL || key == NULL || value == NULL)
		return -2;

	dl_list_for_each(pelem, &pu->argument_list, procutl_cmdline_elem_t, list) {
		if (pelem->var != NULL) {

			if (strcmp(pelem->var, key) == 0) {
				if (pelem->value != NULL) {
					// Has value
					v = (int64_t)strtol(pelem->value, &endptr, 10);
					if (endptr == pelem->value) {
						// Convert fail.
						result = -1;
						goto error_ret;
					} else if (v >= LONG_MAX || v <= LONG_MIN) {
						if (errno == ERANGE) {
							// Convert fail.
							result = -1;
							goto error_ret;
						}
					}
					// Success to convert
					(*value) = v;
					result = 0;
					break;
				}
			}
		}
	}

	return result;

error_ret:

	return result;
}
/**
 * Test key in cmdline.
 *
 * @param [in]	pu		Pointer to get procutil_t object.
 * @param [in]	key		Scanning key.
 * @return int
 * @retval	0	Key is available.
 * @retval	-1	A key is not found.
 * @retval	-2	Argument error.
 */
int procutil_test_key_in_cmdline(procutil_t *pu, const char *key)
{
	int result = -1;
	procutl_cmdline_elem_t *pelem = NULL;

	if (pu == NULL || key == NULL)
		return -2;

	dl_list_for_each(pelem, &pu->argument_list, procutl_cmdline_elem_t, list) {
		if (pelem->var != NULL) {

			if (strcmp(pelem->var, key) == 0) {
				result = 0;
				break;
			}
		}
	}

	return result;
}