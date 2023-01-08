/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	proc-util.h
 * @brief	Header for procfs utility
 */
#ifndef PROC_UTIL_H
#define PROC_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>

//-----------------------------------------------------------------------------
struct s_procutl;
typedef struct s_procutil procutil_t;

int procutil_create(procutil_t **ppu);
int procutil_cleanup(procutil_t *pu);
int procutil_get_cmdline_value_int64(procutil_t *pu, const char *key, int64_t *value);
int procutil_test_key_in_cmdline(procutil_t *pu, const char *key);
//-----------------------------------------------------------------------------
#endif //#ifndef PROC_UTIL_H
