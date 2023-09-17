/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cm-worker-utils.c
 * @brief	container manager utility for worker
 */
#include "cm-worker-utils.h"

#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

int libcmplug_pidfd_open(pid_t pid)
{
	return syscall(SYS_pidfd_open, pid, 0);
}

int libcmplug_pidfd_send_signal(int pidfd, int sig, siginfo_t *info, unsigned int flags)
{
	return syscall(SYS_pidfd_send_signal, pidfd, sig, info, flags);
}

/**
 * Get point to /dev/ trimmed devname.
 *
 * @param [in]	devnode	String to devname with "/dev/" prefix.
 * @return int
 * @retval	!=NULL	Pointer to trimmed devname.
 * @retval	==NULL	Is not devname.
 */
const char *libcmplug_trimmed_devname(const char* devnode)
{
	const char cmpstr[] = "/dev/";
	const char *pstr = NULL;
	size_t cmplen = 0;

	cmplen = strlen(cmpstr);

	if (strncmp(devnode, cmpstr, cmplen) == 0) {
		pstr = &devnode[cmplen];
	}

	return pstr;
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
int libcmplug_node_check(const char *path)
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
 * Get monotonic time counter value by ms resolutions.
 *
 * @return int64_t
 * @retval  >0 current time.
 * @retval -1 Critical error.
 */
int64_t libcmplug_get_current_time_ms(void)
{
	int64_t ms = -1;
	struct timespec t = {0,0};
	int ret = -1;

	ret = clock_gettime(CLOCK_MONOTONIC, &t);
	if (ret == 0) {
		ms = ((int64_t)t.tv_sec * 1000) + ((int64_t)t.tv_nsec / 1000 / 1000);
	}

	return ms;
}
/**
 * Short time sleep.
 *
 * @param [in]	wait_time	sleep time(ms).
 * @return void
 */
void libcmplug_sleep_ms_time(int64_t wait_time)
{
	int ret = -1;
	struct timespec req, rem;

	if (wait_time < 0) {
		return;
	}

	req.tv_sec = wait_time / 1000;	//ms to sec
	req.tv_nsec = (wait_time % 1000) * 1000 * 1000;	//ms to nsec
	rem.tv_sec = 0;
	rem.tv_nsec = 0;

	for(int i=0;i < 10; i++) {	// INTR recover is 10 times.  To avoid no return.
		ret = nanosleep(&req, &rem);
		if ((ret < 0) && (errno == EINTR)) {
			req.tv_sec = rem.tv_sec;
			req.tv_nsec = rem.tv_nsec;
			rem.tv_sec = 0;
			rem.tv_nsec = 0;
			continue;
		}
		break;
	}

	return;
}