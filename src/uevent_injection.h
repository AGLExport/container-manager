/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	uevent_injection.h
 * @brief	The header for uevent injection utility.
 */
#ifndef UEVENT_INJECTION_H
#define UEVENT_INJECTION_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include "container.h"

//-----------------------------------------------------------------------------
int uevent_injection_to_pid(pid_t target_pid, dynamic_device_elem_data_t *dded, char *action);

//-----------------------------------------------------------------------------
#endif //#ifndef UEVENT_INJECTION_H
