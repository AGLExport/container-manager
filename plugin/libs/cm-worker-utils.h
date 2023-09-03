/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cm-worker-utils.h
 * @brief	container manager utility for woker header
 */
#ifndef CM_WORKER_UTIL_H
#define CM_WORKER_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <signal.h>

//-----------------------------------------------------------------------------
int cm_pidfd_open(pid_t pid);
int cm_pidfd_send_signal(int pidfd, int sig, siginfo_t *info, unsigned int flags);

//-----------------------------------------------------------------------------
#endif //#ifndef CM_WORKER_UTIL_H
