/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	devicemng.h
 * @brief	The header for device management data.
 */
#ifndef DEVICE_MNG_H
#define DEVICE_MNG_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <net/if.h>
#include "list.h"
#include "container-control-interface.h"

struct s_dynamic_device_udev;
typedef struct s_dynamic_device_udev dynamic_device_udev_t;

/**
 * @def	DCD_UEVENT_ACTION_NON
 * @brief	The uevent action code for non.  It's for internal use.
 */
#define DCD_UEVENT_ACTION_NON		(0)
/**
 * @def	DCD_UEVENT_ACTION_ADD
 * @brief	The uevent action code for add.  It's for internal use.
 */
#define DCD_UEVENT_ACTION_ADD		(1)
/**
 * @def	DCD_UEVENT_ACTION_REMOVE
 * @brief	The uevent action code for remove.  It's for internal use.
 */
#define DCD_UEVENT_ACTION_REMOVE	(2)
/**
 * @def	DCD_UEVENT_ACTION_CHANGE
 * @brief	The uevent action code for change.  It's for internal use.
 */
#define DCD_UEVENT_ACTION_CHANGE	(3)
/**
 * @def	DCD_UEVENT_ACTION_MOVE
 * @brief	The uevent action code for move.  It's for internal use.
 */
#define DCD_UEVENT_ACTION_MOVE		(4)
/**
 * @def	DCD_UEVENT_ACTION_ONLINE
 * @brief	The uevent action code for online.  It's for internal use.
 */
#define DCD_UEVENT_ACTION_ONLINE	(5)
/**
 * @def	DCD_UEVENT_ACTION_OFFLINE
 * @brief	The uevent action code for offline.  It's for internal use.
 */
#define DCD_UEVENT_ACTION_OFFLINE	(6)
/**
 * @def	DCD_UEVENT_ACTION_BIND
 * @brief	The uevent action code for bind.  It's for internal use.
 */
#define DCD_UEVENT_ACTION_BIND		(7)
/**
 * @def	DCD_UEVENT_ACTION_UNBIND
 * @brief	The uevent action code for unbind.  It's for internal use.
 */
#define DCD_UEVENT_ACTION_UNBIND	(8)


struct s_netifmonitor;
typedef struct s_netifmonitor netifmonitor_t;
//-----------------------------------------------------------------------------
// network interface ---------------------------------
/**
 * @struct	s_network_interface_info
 * @brief	The data structure for network interface, these member data are constructed from netlink RTNL.
 */
struct s_network_interface_info {
	struct dl_list list;		/**< Double link list header. */
	int ifindex;				/**< Dynamic network interface index. */
	char ifname[IFNAMSIZ+1];	/**< Dynamic network interface name into with NULL terminate. */
	//--- internal control data
};
typedef struct s_network_interface_info network_interface_info_t;	/**< typedef for struct s_network_interface_info. */

//-----------------------------------------------------------------------------
/**
 * @struct	s_network_interface_manager
 * @brief	This data structure is manage network interface.
 */
struct s_network_interface_manager {
	struct dl_list nllist;	/**< RTNL based network interface list - network_interface_info_t*/
	struct dl_list devlist;	/**< udev based network interface list - dynamic_device_info_t*/

	//--- internal control data
};
typedef struct s_network_interface_manager network_interface_manager_t;	/**< typedef for struct s_network_interface_manager. */
/**
 * @struct	s_dynamic_device_manager
 * @brief	Central data for dynamic device manager.  It's include each sub block data and pointer to constructed sub data.
 */
struct s_dynamic_device_manager {
	network_interface_manager_t netif;	/**< Management data for network interface. */
	//--- internal control data
	dynamic_device_udev_t *ddu;		/**< Pointer to constructed dynamic_device_udev storage. */
	netifmonitor_t *netifmon;			/**< Pointer to constructed netifmonitor storage. */
};
typedef struct s_dynamic_device_manager dynamic_device_manager_t;	/**< typedef for struct s_dynamic_device_manager. */
//-----------------------------------------------------------------------------
#endif //#ifndef DEVICE_MNG_H
