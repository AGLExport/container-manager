/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cm-util.c
 * @brief	container manager utility functions
 */
#include "cm-utils.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

/**
 * Once write util.
 * This function do three operation by one call. 'open, write and close'
 * This function support only to less than 4KByte operation.
 *
 * @param [in]	path	File path
 * @param [in]	data	Pointer to write data buffer.
 * @param [in]	size	Write data size.
 * @return int
 * @retval  0 Success.
 * @retval -1 Write error.
 */
int once_write(const char *path, const void* data, size_t size)
{
	int fd = -1;
	ssize_t ret = -1;

	fd = open(path, (O_WRONLY | O_CLOEXEC | O_TRUNC));
	if (fd < 0)
		return -1;

	do {
		ret = write(fd, data, size);
	} while (ret == -1 && errno == EINTR);

	close(fd);

	return 0;
}
/**
 * Once read util.
 * This function do three operation by one call. 'open, read and close'
 * This function support only to less than 4KByte operation.
 *
 * @param [in]	path	File path
 * @param [in]	data	Pointer to read data buffer.
 * @param [in]	size	Read data buffer size
 * @return int
 * @retval  0 Success.
 * @retval -1 read error.
 */
int once_read(const char *path, void* data, size_t size)
{
	int fd = -1;
	ssize_t ret = -1;

	fd = open(path, (O_RDONLY | O_CLOEXEC));
	if (fd < 0)
		return -1;

	do {
		ret = read(fd, data, size);
	} while (ret == -1 && errno == EINTR);

	close(fd);

	return 0;
}
/**
 * File node check.
 * Test to path is existing or not.
 * This function is not checked that is directory or file. Only to check existing or not.
 *
 * @param [in]	path	File path
 * @return int
 * @retval  0 Find node.
 * @retval -1 Not find node.
 */
int node_check(const char *path)
{
	int ret = -1;
	struct stat sb = {0};

	ret = stat(path, &sb);
	if (ret < 0) {
		return -1;
	}

	return 0;
}
/**
 * Recursive make dir.
 * This function create directory '/a/b/c/..'.  Similar to  mkdir -p command.
 * When directory '/a' was exist in case of dir equal '/a/b/c', this function is not return exist error.
 *
 * @param [in]	dir		Path of creating directory.
 * @param [in]	mode	Access permission of creating directory. Must be set X flag.
 * @return int
 * @retval  0 Success to create directory.
 * @retval -1 Fail to create directory.
 */
int mkdir_p(const char *dir, mode_t mode)
{
	int ret = -1, result = 0;
	int len = 0;
	char path[PATH_MAX];

	memset(path, 0, sizeof(path));

	len = strlen(dir);

	for(int i=1; i < len; i++) {
		if (path[i] == '/') {
			strncpy(path, dir, (i-1));
			ret = mkdir(path, mode);
			if(ret < 0 && errno != EEXIST) {
				result = -1;
				break;
			}
		}
	}

	return result;
}
/**
 * Wait to exit child process by pid.
 *
 * @param [in]	pid	A pid of waiting child process.
 * @return int
 * @retval  0 A child process was normal exit without error.
 * @retval -1 A child process was abnormal exit such as SIGSEGV, SIGABRT etc.
 * @retval -2 A child process was normal exit with error.
 */
int wait_child_pid(pid_t pid)
{
	pid_t ret = -1;
	int status = 0;

	do {
		ret = waitpid(pid, &status, 0);

	} while (ret < 0 && errno == EINTR);

	if (!WIFEXITED(status))
		return -1;

	if (WEXITSTATUS(status) != 0)
		return -2;

	return 0;
}
/**
 * Get monotonic time counter value by ms resolutions.
 *
 * @return int64_t
 * @retval  >0 current time.
 * @retval -1 Critical error.
 */
int64_t get_current_time_ms(void)
{
	int64_t ms = -1;
	struct timespec t = {0,0};
	int ret = -1;

	ret = clock_gettime(CLOCK_MONOTONIC, &t);
	if (ret == 0) {
		ms = (t.tv_sec * 1000) + (t.tv_nsec / 1000 / 1000);
	}

	return ms;
}