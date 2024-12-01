/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cm-utils.h
 * @brief	container manager utility header
 */
#ifndef CM_UTIL_H
#define CM_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <signal.h>

//-----------------------------------------------------------------------------
int pidfd_open_syscall_wrapper(pid_t pid);
int pidfd_send_signal_syscall_wrapper(int pidfd, int sig, siginfo_t *info, unsigned int flags);
int intr_safe_write(int fd, const void* data, size_t size);
int once_write(const char *path, const void* data, size_t size);
int once_read(const char *path, void* data, size_t size);
int node_check(const char *path);
int mkdir_p(const char *dir, mode_t mode);
int wait_child_pid(pid_t pid);
int64_t get_current_time_ms(void);
void sleep_ms_time(int64_t wait_time);

int mount_disk_failover(char **devs, const char *path, const char *fstype, unsigned long mntflag, char* option);
int mount_disk_ab(char **devs, const char *path, const char *fstype, unsigned long mntflag, char* option, int side);
int mount_disk_once(char **devs, const char *path, const char *fstype, unsigned long mntflag, char* option);
int mount_disk_bind(const char *src_path, const char *dest_path, int is_read_only);
int unmount_disk(const char *path, int64_t timeout_at, int retry_max);
//-----------------------------------------------------------------------------
#endif //#ifndef CM_UTIL_H
