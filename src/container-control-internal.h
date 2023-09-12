/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-control-internal.h
 * @brief	The internal common header for the container management state machine.
 */
#ifndef CONTAINER_CONTROL_INTERNAL_H
#define CONTAINER_CONTROL_INTERNAL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <limits.h>
#include <systemd/sd-event.h>

#include "container.h"
#include "proc-util.h"
//-----------------------------------------------------------------------------
// common definition
//-----------------------------------------------------------------------------
struct s_cm_external_interface;
typedef struct s_cm_external_interface cm_external_interface_t;

/**
 * @struct	s_container_mngsm
 * @brief	The structure for container manager state machine that carry event resource.
 */
struct s_container_mngsm {
	procutil_t *prutl;					/**< Pointer to proc util object. */
	cm_external_interface_t *cm_ext_if;	/**< Pointer to external interface object for container manager. */
	sd_event_source *timer_source;		/**< The sd event source for internal timer. */
	sd_event_source *socket_source;		/**< The sd event source for internal event communication to use receiving event. */
	int secondary_fd;					/**< The file descriptor for internal event communication to use sending event. */
};

//-----------------------------------------------------------------------------
// command packet definition
//-----------------------------------------------------------------------------
/**
 * @def	CONTAINER_MNGSM_COMMAND_BUFSIZEMAX
 * @brief	Buffer size definition for container manager internal event communication.
 */
#define CONTAINER_MNGSM_COMMAND_BUFSIZEMAX (8u*1024u)

/**
 * @typedef	container_mngsm_command_header_t
 * @brief	Typedef for struct s_container_mngsm_command_header.
 */
/**
 * @struct	s_container_mngsm_command_header
 * @brief	Common command packet header for container manager internal event communication.
 */
typedef struct s_container_mngsm_command_header {
	uint32_t command;	/**< Command code of this packet. */
} container_mngsm_command_header_t;

/**
 * @def	CONTAINER_MNGSM_COMMAND_NETIFUPDATED
 * @brief	Defined command code for network interface update notification event.
 */
#define CONTAINER_MNGSM_COMMAND_NETIFUPDATED	(0x2000u)

/**
 * @typedef	container_mngsm_notification_t
 * @brief	Typedef for struct s_container_mngsm_notification.
 */
/**
 * @struct	s_container_mngsm_notification
 * @brief	Defining single event notification packet for container manager internal event communication.
 */
typedef struct s_container_mngsm_notification {
	container_mngsm_command_header_t header;	/**< Header for this notification packet. */
} container_mngsm_notification_t;

/**
 * @def	CONTAINER_MNGSM_COMMAND_GUEST_EXIT
 * @brief	Defined command code for container exit notification event.
 */
#define CONTAINER_MNGSM_COMMAND_GUEST_EXIT	(0x3000u)

/**
 * @typedef	container_mngsm_guest_exit_data_t
 * @brief	Typedef for struct s_container_mngsm_guest_exit_data.
 */
/**
 * @struct	s_container_mngsm_guest_exit_data
 * @brief	Defining data block for container exit notification packet.
 */
typedef struct s_container_mngsm_guest_exit_data {
	int container_number;	/**< Exited guest container number. */
} container_mngsm_guest_exit_data_t;

/**
 * @typedef	container_mngsm_guest_status_exit_t
 * @brief	Typedef for struct s_container_mngsm_guest_exit.
 */
/**
 * @struct	s_container_mngsm_guest_exit
 * @brief	Defining container exit notification packet for container manager internal event communication.
 */
typedef struct s_container_mngsm_guest_exit {
	container_mngsm_command_header_t header;	/**< Header for this notification packet. */
	container_mngsm_guest_exit_data_t data;		/**< Data for this notification packet. */
} container_mngsm_guest_status_exit_t;

/**
 * @def	CONTAINER_MNGSM_COMMAND_SYSTEM_SHUTDOWN
 * @brief	Defined command code for received system shutdown notification event.
 */
#define CONTAINER_MNGSM_COMMAND_SYSTEM_SHUTDOWN	(0x4000u)
/**
 * @def	CONTAINER_MNGSM_COMMAND_TIMER_TICK
 * @brief	Defined command code for timer tick event.
 */
#define CONTAINER_MNGSM_COMMAND_TIMER_TICK		(0x5000u)

//-----------------------------------------------------------------------------

int container_netif_updated(containers_t *cs);
int container_exited(containers_t *cs, const container_mngsm_guest_exit_data_t *data);
int container_manager_shutdown(containers_t *cs);
int container_exec_internal_event(containers_t *cs);
int container_request_shutdown(container_config_t *cc, int sys_state);
int container_request_reboot(container_config_t *cc, int sys_state);

int container_all_dynamic_device_update_notification(containers_t *cs);

int container_monitor_addguest(containers_t *cs, container_config_t *cc);

int container_start_by_role(containers_t *cs, char *role);
int container_start(container_config_t *cc);
int container_terminate(container_config_t *cc);
int container_cleanup(container_config_t *cc, int64_t timeout);

//-----------------------------------------------------------------------------
#endif //#ifndef CONTAINER_CONTROL_INTERNAL_H
