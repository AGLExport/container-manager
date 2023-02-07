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
#include "container.h"
//-----------------------------------------------------------------------------
int device_control_dynamic_udev_setup(dynamic_device_manager_t *ddm, containers_t *cs, sd_event *event);
int device_control_dynamic_udev_cleanup(dynamic_device_manager_t *ddm);

//-----------------------------------------------------------------------------
#endif //#ifndef UDEV_UTIL_H
