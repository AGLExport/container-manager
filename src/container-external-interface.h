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

/**
 * @struct	s_cm_external_interface
 * @brief	The structure for container manager external interface that carry event resource.
 */
struct s_cm_external_interface {
	sd_event *parent_eventloop;						/**< Reference for parent event loop. */
	sd_event_source *interface_evsource;			/**< A event source for container manager external interface. */
	sd_event_source *interface_session_evsource;	/**< A event source for external interface session. (Single session) */
    containers_t *cs;								/**< Pointer to container manager state machine object. (Reference only) */
};
typedef struct s_cm_external_interface cm_external_interface_t;	/**< typedef for struct s_cm_external_interface. */

int container_external_interface_setup(containers_t *cs, sd_event *event);
int container_external_interface_cleanup(containers_t *cs);
//-----------------------------------------------------------------------------
#endif //#ifndef CONTAINER_EXTERNAL_INTERFACE_H
