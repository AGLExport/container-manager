/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cgroup-utils.h
 * @brief	The header of cgroup utility for container manager.
 */
#ifndef CGROUP_UTILS_H
#define CGROUP_UTILS_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

//-----------------------------------------------------------------------------
int cgroup_util_get_cgroup_version(void);
int cgroup_util_cgroup_v2_setup(void);
//-----------------------------------------------------------------------------
#endif //#ifndef CGROUP_UTILS_H