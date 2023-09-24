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
int libcmplug_pidfd_open(pid_t pid);
int libcmplug_pidfd_send_signal(int pidfd, int sig, siginfo_t *info, unsigned int flags);
const char *libcmplug_trimmed_devname(const char* devnode);
int libcmplug_node_check(const char *path);
int64_t libcmplug_get_current_time_ms(void);
void libcmplug_sleep_ms_time(int64_t wait_time);
//-----------------------------------------------------------------------------
#endif //#ifndef CM_WORKER_UTIL_H
