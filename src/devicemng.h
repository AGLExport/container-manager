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

struct s_udevmonitor;
typedef struct s_udevmonitor udevmonitor_t;

struct s_netifmonitor;
typedef struct s_netifmonitor netifmonitor_t;
//-----------------------------------------------------------------------------
// udev device ---------------------------------
/**
 * @struct	s_dynamic_device_info
 * @brief	The data structure for dynamic device, these member data are constructed from uevent.
 */
struct s_dynamic_device_info {
	struct dl_list list;	/**< Double link list header. */
	char *syspath;			/**< sysfs path for this device. */
	char *sysname;			/**< sysname fo this device. */
	char *devpath;			/**< device path for this device. */
	char *devtype;			/**< device type for this device. */
	char *subsystem;		/**< subsystem name for this device. */
	char *devnode;			/**< device node name for this device. - char/block device only */
	dev_t devnum;			/**< device major and minor number for this device. - char/block device only */
	char *diskseq;			/**< diskseq for this device. - block device - disk/partition only */
	char *partn;			/**< partition num for this device. - block device - partition only */
	int ifindex;			/**< network interface index - net only */
	//-- drop data
	//char *sysnum; -- from udev_device_get_sysnum
	//char *driver; -- from udev_device_get_driver
	//--- internal control data
	uint32_t fsmagic;		/**< file system magic code. 0 is no fs> */
};
typedef struct s_dynamic_device_info dynamic_device_info_t;	/**< typedef for struct s_dynamic_device_info. */
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
 * @struct	s_block_device_manager
 * @brief	This data structure is manage dynamic block device.
 */
struct s_block_device_manager {
	struct dl_list list;	/**< device list for disk/partition - dynamic_device_info_t*/
	//--- internal control data
};
typedef struct s_block_device_manager block_device_manager_t;	/**< typedef for struct s_block_device_manager. */

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
	block_device_manager_t blockdev;	/**< Management data for block device. */
	network_interface_manager_t netif;	/**< Management data for network interface. */
	//--- internal control data
	udevmonitor_t *udevmon;				/**< Pointer to constructed udevmonitor storage. */
	netifmonitor_t *netifmon;			/**< Pointer to constructed netifmonitor storage. */
};
typedef struct s_dynamic_device_manager dynamic_device_manager_t;	/**< typedef for struct s_dynamic_device_manager. */
//-----------------------------------------------------------------------------
#endif //#ifndef DEVICE_MNG_H
