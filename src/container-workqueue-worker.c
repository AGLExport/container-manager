/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-workqueue-worker.c
 * @brief	This file include implementation for worker operations.
 */
#include "container-workqueue-worker.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/**
 * @brief Function pointer for container workqueue.
 *
 * @return Description for return value
 * @retval 0	Success to execute worker.
 * @retval -1	Fail to execute worker.
 */
int container_worker_test(void)
{
	uint64_t buff[1024*1024/sizeof(uint64_t)];
	int fd = -1;
	int ret = -1;

	fd = open("/dev/mmcblk1p7", O_CLOEXEC | O_SYNC | O_WRONLY);
	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout,"container_worker_test: open ret = %d\n", fd);
	#endif
	if (fd >= 0) {
		ssize_t sret = -1;
		memset(buff, 0 , sizeof(buff));

		do {
			sret = write(fd, buff, sizeof(buff));
		} while(sret > 0 && errno != EINTR);

		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"container_worker_test: write end for /dev/mmcblk1p7 ret = %d(%d)\n",(int)sret, errno);
		#endif
		close(fd);

		ret = 0;
	}

	return ret;
}

