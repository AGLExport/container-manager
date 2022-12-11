/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cm-util.h
 * @brief	container manager utility header
 */
#ifndef CM_UTIL_H
#define CM_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

//-----------------------------------------------------------------------------
int onece_write(const char *path, const void* data, size_t size);
int onece_read(const char *path, void* data, size_t size);
int node_check(const char *path);
int mkdir_p(const char *dir, mode_t mode);
int wait_child_pid(pid_t pid);
int64_t get_current_time_ms(void);

//-----------------------------------------------------------------------------
#endif //#ifndef CM_UTIL_H
