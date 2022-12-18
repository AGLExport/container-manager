/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-external-interface.h
 * @brief	container external interface header
 */
#ifndef CONTAINER_EXTERNAL_INTERFACE_H
#define CONTAINER_EXTERNAL_INTERFACE_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <systemd/sd-event.h>

//-----------------------------------------------------------------------------
struct s_containers;
typedef struct s_containers containers_t;

/** Container manager external interface */
struct s_cm_external_interface {
	sd_event *parent_eventloop;	  /**< UNIX Domain socket event source for data pool service */
	sd_event_source *interface_evsource; /**< UNIX Domain socket event source for data pool service */
	sd_event_source *interface_session_evsource; /**< Client sessions */
    containers_t *cs;
};
typedef struct s_cm_external_interface cm_external_interface_t;

int container_external_interface_setup(containers_t *cs, sd_event *event);
int container_external_interface_cleanup(containers_t *cs);
//-----------------------------------------------------------------------------
#endif //#ifndef CONTAINER_EXTERNAL_INTERFACE_H
