/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cm-worker-utils.c
 * @brief	container manager utility for worker
 */
#include "cm-worker-utils.h"

#include <sys/syscall.h>
#include <unistd.h>

int cm_pidfd_open(pid_t pid)
{
	return syscall(SYS_pidfd_open, pid, 0);
}

int cm_pidfd_send_signal(int pidfd, int sig, siginfo_t *info, unsigned int flags)
{
	return syscall(SYS_pidfd_send_signal, pidfd, sig, info, flags);
}
