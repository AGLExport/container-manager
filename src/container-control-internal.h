/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-control-internal.h
 * @brief	internal header for the container management state machine.
 */
#ifndef CONTAINER_CONTROL_INTERNAL_H
#define CONTAINER_CONTROL_INTERNAL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <limits.h>

//-----------------------------------------------------------------------------
// common definition
//-----------------------------------------------------------------------------
struct s_container_mngsm {
	sd_event_source *socket_source;
	int primary_fd;
	int secondary_fd;
};

//-----------------------------------------------------------------------------
// command packet definition
//-----------------------------------------------------------------------------
#define CONTAINER_MNGSM_COMMAND_BUFSIZEMAX (8*1024)

typedef struct s_container_mngsm_command_header {
	uint32_t command;
} container_mngsm_command_header_t;

#define CONTAINER_MNGSM_COMMAND_DEVICEUPDATED	(0x1000u)
#define CONTAINER_MNGSM_COMMAND_NETIFUPDATED	(0x2000u)

typedef struct s_container_mngsm_notification {
	container_mngsm_command_header_t header;
} container_mngsm_notification_t;

#define CONTAINER_MNGSM_COMMAND_GUEST_EXIT	(0x3000u)

typedef struct s_container_mngsm_guest_exit_data {
	int container_number;
} container_mngsm_guest_exit_data_t;

typedef struct s_container_mngsm_guest_exit {
	container_mngsm_command_header_t header;
	container_mngsm_guest_exit_data_t data;
} container_mngsm_guest_status_exit_t;

#define CONTAINER_MNGSM_COMMAND_SYSTEM_SHUTDOWN	(0x4000u)



//-----------------------------------------------------------------------------


int container_device_updated(containers_t *cs);
int container_netif_updated(containers_t *cs);
int container_exited(containers_t *cs, container_mngsm_guest_exit_data_t *data);
int container_manager_shutdown(containers_t *cs);
int container_exec_internal_event(containers_t *cs);

int container_start(container_config_t *cc);
int container_terminate(container_config_t *cc);

//-----------------------------------------------------------------------------
#endif //#ifndef CONTAINER_CONTROL_INTERNAL_H
