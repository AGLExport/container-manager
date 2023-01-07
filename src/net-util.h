/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	net-util.h
 * @brief	The header for netlink utility.
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
