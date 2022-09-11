/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	util.h
 * @brief	utility header
 */
#ifndef NET_UTIL_H
#define NET_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <systemd/sd-event.h>
#include "devicemng.h"

//-----------------------------------------------------------------------------
int netifmonitor_setup(dynamic_device_manager_t *ddm, container_control_interface_t *cci, sd_event *event);
int netifmonitor_cleanup(dynamic_device_manager_t *ddm);

//-----------------------------------------------------------------------------
#endif //#ifndef NET_UTIL_H
