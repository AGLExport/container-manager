/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-manager-interface.h
 * @brief	container manager external interface header
 */
#ifndef CONTAINER_MANAGER_EXTERNAL_INTERFACE_H
#define CONTAINER_MANAGER_EXTERNAL_INTERFACE_H
//-----------------------------------------------------------------------------
#include <stdint.h>

//-----------------------------------------------------------------------------
#define CONTAINER_MANAGER_EXTERNAL_SOCKET_NAME ("\0agl/container-manager-interface")

//-----------------------------------------------------------------------------
// Command packet
#define CONTAINER_EXTIF_COMMAND_BUFSIZEMAX (8*1024)

#define CONTAINER_EXTIF_STR_LEN_MAX (128)
#define CONTAINER_EXTIF_GUESTS_MAX (8*2) //Ref. to container.h GUEST_CONTAINER_LIMIT
//-----------------------------------------------------------------------------
// Client -> Container manager
typedef struct s_container_extif_command_header {
	uint32_t command;
} container_extif_command_header_t;

#define CONTAINER_EXTIF_COMMAND_GETGUESTS       (0x1000u)
typedef struct s_container_extif_command_get {
	container_extif_command_header_t header;
} container_extif_command_get_t;


#define CONTAINER_EXTIF_COMMAND_LIFECYCLE_GUEST_NAME  (0x2000u)
#define CONTAINER_EXTIF_COMMAND_LIFECYCLE_GUEST_ROLE  (0x2001u)
#define CONTAINER_EXTIF_SUBCOMMAND_SHUTDOWN_GUEST  (0x0001u)
#define CONTAINER_EXTIF_SUBCOMMAND_REBOOT_GUEST  (0x0002u)
#define CONTAINER_EXTIF_SUBCOMMAND_FORCEREBOOT_GUEST  (0x0003u)
typedef struct s_container_extif_command_lifecycle {
	container_extif_command_header_t header;
    uint32_t subcommand;
    char guest_name[CONTAINER_EXTIF_STR_LEN_MAX];
} container_extif_command_lifecycle_t;

#define CONTAINER_EXTIF_COMMAND_CHANGE_ACTIVE_GUEST_NAME  (0x3000u)
typedef struct s_container_extif_command_change {
	container_extif_command_header_t header;
    char guest_name[CONTAINER_EXTIF_STR_LEN_MAX];
} container_extif_command_change_t;

//-----------------------------------------------------------------------------
// Container manager -> Client
typedef struct s_container_extif_command_response_header {
	uint32_t command;
} container_extif_command_response_header_t;

#define CONTAINER_EXTIF_COMMAND_RESPONSE_GETGUESTS       (0xa1000u)
typedef struct s_container_extif_guests_info {
    char guest_name[CONTAINER_EXTIF_STR_LEN_MAX];
    char role_name[CONTAINER_EXTIF_STR_LEN_MAX];
    int32_t status;
} container_extif_guests_info_t;

typedef struct s_container_extif_command_get_response {
	container_extif_command_response_header_t header;
    container_extif_guests_info_t guests[CONTAINER_EXTIF_GUESTS_MAX];
    int32_t num_of_guests;
} container_extif_command_get_response_t;

#define CONTAINER_EXTIF_GUEST_STATUS_DISABLE		(-1)
#define CONTAINER_EXTIF_GUEST_STATUS_NOT_STARTED	(0)
#define CONTAINER_EXTIF_GUEST_STATUS_STARTED		(1)
#define CONTAINER_EXTIF_GUEST_STATUS_SHUTDOWN		(2)
#define CONTAINER_EXTIF_GUEST_STATUS_DEAD			(3)
#define CONTAINER_EXTIF_GUEST_STATUS_EXIT			(4)

#define CONTAINER_EXTIF_COMMAND_RESPONSE_LIFECYCLE      (0xa2000u)
typedef struct s_container_extif_command_lifecycle_response {
	container_extif_command_response_header_t header;
    int32_t response;
} container_extif_command_lifecycle_response_t;

#define CONTAINER_EXTIF_LIFECYCLE_RESPONSE_ACCEPT   (0)
#define CONTAINER_EXTIF_LIFECYCLE_RESPONSE_NONAME   (-1)
#define CONTAINER_EXTIF_LIFECYCLE_RESPONSE_NOROLE   (-2)
#define CONTAINER_EXTIF_LIFECYCLE_RESPONSE_ERROR    (-100)

#define CONTAINER_EXTIF_COMMAND_RESPONSE_CHANGE      (0xa3000u)
typedef struct s_container_extif_command_change_response {
	container_extif_command_response_header_t header;
    int32_t response;
} container_extif_command_change_response_t;

#define CONTAINER_EXTIF_CHANGE_RESPONSE_ACCEPT   (0)
#define CONTAINER_EXTIF_CHANGE_RESPONSE_NONAME   (-1)
#define CONTAINER_EXTIF_CHANGE_RESPONSE_ERROR    (-100)


//-----------------------------------------------------------------------------
#endif //#ifndef CONTAINER_MANAGER_EXTERNAL_INTERFACE_H
