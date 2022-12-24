/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container.h
 * @brief	container management data
 */
#ifndef CONTAINER_H
#define CONTAINER_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include "list.h"

#include <lxc/lxccontainer.h>
#include <systemd/sd-event.h>
#include "devicemng.h"
#include "manager.h"
//-----------------------------------------------------------------------------
// Base config ---------------------------------
#define DISKMOUNT_TYPE_RO	(0)
#define DISKMOUNT_TYPE_RW	(1)
#define DISKREDUNDANCY_TYPE_FAILOVER	(0)
#define DISKREDUNDANCY_TYPE_AB	(1)

struct s_container_baseconfig_rootfs {
	char *path;	 		/**< rootfs mount path in host */
	char *filesystem;	/**< rootfs file system type */
	int mode;	/**< file system mount mode (ro/rw) */
	char *blockdev[2];	/**< block device for rootfs with A/B update 0=a.1=b */
};
typedef struct s_container_baseconfig_rootfs container_baseconfig_rootfs_t;

struct s_container_baseconfig_extradisk {
	struct dl_list list;
	char *from;	 		/**< persistence mount path in host */
	char *to;	 		/**< persistence mount path in guest */
	char *filesystem;	/**< rootfs file system type */
	int mode;	/**< file system mount mode (ro/rw) */
	int	redundancy;	/**< redundancy mode (failover/ab) */
	char *blockdev[2];	/**< block device for rootfs primary and secondary */
};
typedef struct s_container_baseconfig_extradisk container_baseconfig_extradisk_t;

struct s_container_baseconfig_lifecycle {
	char *halt;		/**< Shutdown request signal for guest */
	char *reboot;	/**< Reboot request signal for guest */
	int timeout;	/**< Shutdown timeout for guest */
};
typedef struct s_container_baseconfig_lifecycle container_baseconfig_lifecycle_t;

struct s_container_baseconfig_capability {
	char *drop;	/**< Drop capabilities */
	char *keep;	/**< Keep capabilities */
};
typedef struct s_container_baseconfig_capability container_baseconfig_capability_t;

struct s_container_baseconfig_idmap {
	int guest_root_id;	/**< Gest root id. Typically 0 */
	int host_start_id;	/**< ID mapping start point in host */
	int num_of_id;	/**< Num of id using guest */
};
typedef struct s_container_baseconfig_idmap container_baseconfig_idmap_t;

struct s_container_baseconfig_idmaps {
	int enabled;	/** < Enable id mapping */
	container_baseconfig_idmap_t uid;
	container_baseconfig_idmap_t gid;
};
typedef struct s_container_baseconfig_idmaps container_baseconfig_idmaps_t;

struct s_container_baseconfig_env {
	struct dl_list list;
	char *envstring;	/** < Environment variable list */
};
typedef struct s_container_baseconfig_env container_baseconfig_env_t;

struct s_container_baseconfig {
	int	autoboot;
	int bootpriority;
	container_baseconfig_rootfs_t rootfs;
	struct dl_list extradisk_list;
	container_baseconfig_lifecycle_t lifecycle;
	container_baseconfig_capability_t cap;
	container_baseconfig_idmaps_t idmaps;
	struct dl_list envlist;
	//--- internal control data
	int abboot;
};
typedef struct s_container_baseconfig container_baseconfig_t;
//-----------------------------------------------------------------------------
// resource config ---------------------------------
#define RESOURCE_TYPE_CGROUP	(1)

struct s_container_resource_elem {
	struct dl_list list;
	int type;	/** < resource control type */
	char *object;	/** < resource object */
	char *value;	/** < value for resource object */
};
typedef struct s_container_resource_elem container_resource_elem_t;

struct s_container_resource {
	struct dl_list resourcelist;
};
typedef struct s_container_resource container_resource_t;

struct s_container_resourceconfig {
	container_resource_t resource;
};
typedef struct s_container_resourceconfig container_resourceconfig_t;
//-----------------------------------------------------------------------------
// fs config ---------------------------------
#define FSMOUNT_TYPE_FILESYSTEM	(1)
#define FSMOUNT_TYPE_DIRECTORY	(2)

struct s_container_fsmount_elem {
	struct dl_list list;
	int type;	/** < mount type */
	char *from;	/** < host side mount entry  */
	char *to;	/** < guest side mount entry  */
	char *fstype;	/** < file system type */
	char *option;	/** < mount option  */
};
typedef struct s_container_fsmount_elem container_fsmount_elem_t;

struct s_container_fsmount {
	struct dl_list mountlist;
};
typedef struct s_container_fsmount container_fsmount_t;

struct s_container_fsconfig {
	container_fsmount_t fsmount;
};
typedef struct s_container_fsconfig container_fsconfig_t;
//-----------------------------------------------------------------------------
// device config ---------------------------------
#define DEVICE_TYPE_DEVNODE	(1)
#define DEVICE_TYPE_DEVDIR	(2)
#define DEVICE_TYPE_GPIO	(3)
#define DEVICE_TYPE_IIO		(4)

#define DEVNODE_TYPE_CHR	(1)
#define DEVNODE_TYPE_BLK	(2)

struct s_container_static_device_elem {
	struct dl_list list;
	int type;	/** < device type - DEVICE_TYPE_x */
	char *from;	/** < host side mount entry  */
	char *to;	/** < guest side mount entry  */
	char *devnode;	/** < Check device node */
	int optional;	/** < 0 = required, 1 = optional  */
	int wideallow;	/** < mount type */
	int exclusive;	/** < exclusive assign to guest */
	//--- internal control data
	int is_valid;	/** < static device was available */
	int devtype;	/** < device driver type - DEVNODE_TYPE_x */
	int major;	/** < major number */
	int minor;	/** < minor number */
};
typedef struct s_container_static_device_elem container_static_device_elem_t;

#define DEVGPIO_DIRECTION_DC	(0)
#define DEVGPIO_DIRECTION_IN	(1)
#define DEVGPIO_DIRECTION_OUT	(2)
#define DEVGPIO_DIRECTION_LOW	(3)
#define DEVGPIO_DIRECTION_HIGH	(4)
inline static int devgpio_direction_isvalid(int x) {
	int ret = -1;

	if ((x >= DEVGPIO_DIRECTION_DC) && (x <= DEVGPIO_DIRECTION_HIGH))
		ret = 1;

	return ret;
}

struct s_container_static_gpio_elem {
	struct dl_list list;
	int type;	/** < device type */
	int port;	/** < gpio port num */
	int portdirection;	/** < gpio port direction */
	char *from;	/** < host side mount entry  */
	char *to;	/** < guest side mount entry  */
	//--- internal control data
	int is_valid;	/** < gpio port available */
};
typedef struct s_container_static_gpio_elem container_static_gpio_elem_t;

struct s_container_static_iio_elem {
	struct dl_list list;
	int type;	/** < device type */
	char *sysfrom;	/** < host side mount entry sysfs */
	char *systo;	/** < guest side mount entry sysfs */
	char *devfrom;	/** < host side mount entry dev */
	char *devto;	/** < guest side mount entry dev */
	char *devnode;	/** < Check device node */
	int optional;	/** < 0 = required, 1 = optional  */
	//--- internal control data
	int is_sys_valid;	/** < sysfs node available */
	int is_dev_valid;	/** < device node available */
	int major;	/** < major number */
	int minor;	/** < minor number */
};
typedef struct s_container_static_iio_elem container_static_iio_elem_t;

struct s_container_static_device {
	struct dl_list static_devlist;
	struct dl_list static_gpiolist;
	struct dl_list static_iiolist;
};
typedef struct s_container_static_device container_static_device_t;

struct s_dynamic_device_elem_data {
	struct dl_list list;
	char *devpath;	/** < devpath */
	char *devtype;	/** < device type */
	char *subsystem;	/** < subsystem name */
	char *devnode;	/** < device node name  - char/block device only */
	dev_t devnum;	/** < device major and minor number - char/block device only */
	char *diskseq;	/** < diskseq - block device - disk/partition only */
	char *partn;	/** < partition num - block device - partition only */
	//--- internal control data
	int is_available;
};
typedef struct s_dynamic_device_elem_data dynamic_device_elem_data_t;

struct s_container_dynamic_device_elem {
	struct dl_list list;
	char *devpath;
	char *subsystem;
	char *devtype;
	int mode;
	//--- internal control data
	struct dl_list device_list;
};
typedef struct s_container_dynamic_device_elem container_dynamic_device_elem_t;

struct s_container_dynamic_device {
	struct dl_list dynamic_devlist;
};
typedef struct s_container_dynamic_device container_dynamic_device_t;

struct s_container_deviceconfig {
	container_static_device_t static_device;
	container_dynamic_device_t dynamic_device;
};
typedef struct s_container_deviceconfig container_deviceconfig_t;

//-----------------------------------------------------------------------------
// network interface ---------------------------------
#define STATICNETIF_VETH	(1)

struct s_netif_elem_veth {
	char *name;
	char *link;
	char *flags;
	char *hwaddr;
	char *mode;
	char *address;
	char *gateway;
};
typedef struct s_netif_elem_veth netif_elem_veth_t;

struct s_container_static_netif_elem {
	struct dl_list list;
	int type;
	void *setting;
};
typedef struct s_container_static_netif_elem container_static_netif_elem_t;

struct s_container_static_netif {
	struct dl_list static_netiflist;
};
typedef struct s_container_static_netif container_static_netif_t;

struct s_container_dynamic_netif_elem {
	struct dl_list list;
	char *ifname;
	//--- internal control data
	int ifindex;
	int is_available;
};
typedef struct s_container_dynamic_netif_elem container_dynamic_netif_elem_t;

struct s_container_dynamic_netif {
	struct dl_list dynamic_netiflist;
};
typedef struct s_container_dynamic_netif container_dynamic_netif_t;

struct s_container_netifconfig {
	container_static_netif_t static_netif;
	container_dynamic_netif_t dynamic_netif;
};
typedef struct s_container_netifconfig container_netifconfig_t;

//-----------------------------------------------------------------------------
// runtime status ---------------------------------
#define CONTAINER_DISABLE		(0)
#define CONTAINER_NOT_STARTED	(1)
#define CONTAINER_STARTED		(2)
#define CONTAINER_SHUTDOWN		(3)
#define CONTAINER_DEAD			(4)
#define CONTAINER_EXIT			(5)

struct s_container_runtime_status {
	struct lxc_container *lxc;
	int64_t timeout;
	int status;
	pid_t pid;
	sd_event_source *pidfd_source;
};
typedef struct s_container_runtime_status container_runtime_status_t;
//-----------------------------------------------------------------------------
struct s_container_config {
	char *name;
	char *role;
	container_baseconfig_t baseconfig;
	container_resourceconfig_t resourceconfig;
	container_fsconfig_t fsconfig;
	container_deviceconfig_t deviceconfig;
	container_netifconfig_t netifconfig;
	//--- internal control data
	container_runtime_status_t runtime_stat;
};
typedef struct s_container_config container_config_t;
//-----------------------------------------------------------------------------
#define GUEST_CONTAINER_LIMIT	(8)	/** < Limit value for containers */

struct s_container_mngsm;
typedef struct s_container_mngsm container_mngsm_t;

struct s_container_control_interface;
typedef struct s_container_control_interface container_control_interface_t;

#define CM_SYSTEM_STATE_RUN			(0)
#define CM_SYSTEM_STATE_SHUTDOWN	(1)

struct s_containers {
	container_manager_config_t *cmcfg;	/** < Global config for container manager*/

	int num_of_container;	/** < Num of container data */
	int sys_state;
	container_config_t **containers;	/** < container config array */

	container_mngsm_t *cms;	/** < container management state machine */
	container_control_interface_t *cci;	/** < container control interface */
	dynamic_device_manager_t *ddm;	/** < dynamic device manager */

	sd_event *event;
};
typedef struct s_containers containers_t;
//-----------------------------------------------------------------------------

#endif //#ifndef CONTAINER_H
