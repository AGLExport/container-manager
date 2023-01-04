/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	udev-util.h
 * @brief	The header for device management functions using libudev.
 */
#ifndef UDEV_UTIL_H
#define UDEV_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <systemd/sd-event.h>
#include "devicemng.h"
//-----------------------------------------------------------------------------
int udevmonitor_setup(dynamic_device_manager_t *ddm, container_control_interface_t *cci, sd_event *event);
int udevmonitor_cleanup(dynamic_device_manager_t *ddm);

//-----------------------------------------------------------------------------
#endif //#ifndef UDEV_UTIL_H
