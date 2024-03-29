/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-control.h
 * @brief	Header for container manager state machine functions.
 */
#ifndef CONTAINER_CONTROL_H
#define CONTAINER_CONTROL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <systemd/sd-event.h>
#include "container.h"
#include "container-control-interface.h"

//-----------------------------------------------------------------------------
int container_mngsm_start(containers_t *cs);
int container_mngsm_exit(containers_t *cs);
int container_mngsm_terminate(containers_t *cs);

int container_mngsm_setup(containers_t **pcs, sd_event *event, const char *config_dir);
int container_mngsm_cleanup(containers_t *cs);
int container_mngsm_regist_device_manager(containers_t *cs, dynamic_device_manager_t *ddm);
int container_mngsm_update_timertick(containers_t *cs);

int container_mngsm_interface_get(container_control_interface_t **pcci, containers_t *cs);
int container_mngsm_interface_free(containers_t *cs);

int container_mngsm_exec_delayed_operation(containers_t *cs, int role);
int container_mngsm_do_cyclic_operation(containers_t *cs);
//-----------------------------------------------------------------------------
#endif //#ifndef CONTAINER_CONTROL_H
