/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	devicemng.h
 * @brief	device management data
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
typedef struct s_udevmonitor* udevmonitor_t;

struct s_netifmonitor;
typedef struct s_netifmonitor* netifmonitor_t;
//-----------------------------------------------------------------------------
// udev device ---------------------------------
struct s_dynamic_device_info {
	struct dl_list list;
	char *syspath;	/** < sysfs path */
	char *sysname;	/** < sysname  */
	char *devpath;	/** < devpath */
	char *devtype;	/** < device type */
	char *subsystem;	/** < subsystem name */
	char *devnode;	/** < device node name  - char/block device only */
	dev_t devnum;	/** < device major and minor number - char/block device only */
	char *diskseq;	/** < diskseq - block device - disk/partition only */
	char *partn;	/** < partition num - block device - partition only */
	int ifindex;	/** < network interface index - net only */
	//-- drop data
	//char *sysnum; -- from udev_device_get_sysnum
	//char *driver; -- from udev_device_get_driver

	//--- internal control data
	int assigned_container_index;
	uint32_t fsmagic;	/** < file system magic code. 0 is no fs> */
};
typedef struct s_dynamic_device_info dynamic_device_info_t;
//-----------------------------------------------------------------------------
// network interface ---------------------------------

struct s_network_interface_info {
	struct dl_list list;
	int ifindex;
	char ifname[IFNAMSIZ+1];

	//--- internal control data
	int assigned_container_index;
};
typedef struct s_network_interface_info network_interface_info_t;

//-----------------------------------------------------------------------------
struct s_block_device_manager {
	struct dl_list list;	/** < device list for disk/partition - dynamic_device_info_t*/

	//--- internal control data
};
typedef struct s_block_device_manager block_device_manager_t;

//-----------------------------------------------------------------------------
struct s_network_interface_manager {
	struct dl_list nllist;	/** < RTNL based network interface list - network_interface_info_t*/
	struct dl_list devlist;	/** < udev based network interface list - dynamic_device_info_t*/

	//--- internal control data
};
typedef struct s_network_interface_manager network_interface_manager_t;


struct s_dynamic_device_manager {
	block_device_manager_t blockdev;
	network_interface_manager_t netif;
	//--- internal control data

	udevmonitor_t *udevmon;
	netifmonitor_t *netifmon;

};
typedef struct s_dynamic_device_manager dynamic_device_manager_t;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#endif //#ifndef DEVICE_MNG_H
