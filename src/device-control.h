/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	lxc-util.h
 * @brief	lxc utility header
 */
#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <systemd/sd-event.h>
#include "container.h"
#include "devicemng.h"

//-----------------------------------------------------------------------------
int devc_early_device_setup(containers_t *cs);


int devc_device_manager_setup(dynamic_device_manager_t **pddm, container_control_interface_t *cci, sd_event *event);
int devc_device_manager_cleanup(dynamic_device_manager_t *ddm);

int dynamic_block_device_info_get(block_device_manager_t **blockdev,dynamic_device_manager_t *ddm);
int network_interface_info_get(network_interface_manager_t **netif, dynamic_device_manager_t *ddm);

//-----------------------------------------------------------------------------
#endif //#ifndef DEVICE_CONTROL_H
