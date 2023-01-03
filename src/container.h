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
/**
 * @def	DISKMOUNT_TYPE_RO
 * @brief	Disk mount type is read only. It use at s_container_baseconfig_rootfs.mode and s_container_baseconfig_extradisk.mode.
 */
#define DISKMOUNT_TYPE_RO	(0)
/**
 * @def	DISKMOUNT_TYPE_RW
 * @brief	Disk mount type is read write. It use at s_container_baseconfig_rootfs.mode and s_container_baseconfig_extradisk.mode.
 */
#define DISKMOUNT_TYPE_RW	(1)
/**
 * @def	DISKREDUNDANCY_TYPE_FAILOVER
 * @brief	Disk mount redundancy type is fail over (1st disk mount was fail, try to mount 2nd disk). It use at s_container_baseconfig_extradisk.redundancy.
 */
#define DISKREDUNDANCY_TYPE_FAILOVER	(0)
/**
 * @def	DISKREDUNDANCY_TYPE_AB
 * @brief	Disk mount redundancy type is AB (Automatically select A or B depend on boot parameter). It use at s_container_baseconfig_extradisk.redundancy.
 */
#define DISKREDUNDANCY_TYPE_AB	(1)

/**
 * @struct	s_container_baseconfig_rootfs
 * @brief	The data structure for container root filesystem.  It's a part of s_container_baseconfig.
 */
struct s_container_baseconfig_rootfs {
	char *path;			/**< rootfs mount path in host. */
	char *filesystem;	/**< rootfs file system type. */
	int mode;			/**< file system mount mode. (ro=DISKMOUNT_TYPE_RO/rw=DISKMOUNT_TYPE_RW) */
	char *blockdev[2];	/**< block device for rootfs with A/B update. 0=a.1=b */
};
typedef struct s_container_baseconfig_rootfs container_baseconfig_rootfs_t;	/**< typedef for struct s_container_baseconfig_rootfs. */

/**
 * @struct	s_container_baseconfig_extradisk
 * @brief	The data structure for container extra disks.  It's a list element for extradisk_list of s_container_baseconfig.
 */
struct s_container_baseconfig_extradisk {
	struct dl_list list;	/**< Double link list header. */
	char *from;	 			/**< extra disk mount path in host. */
	char *to;				/**< extra disk mount path in guest. */
	char *filesystem;		/**< file system type for extra disk. (ex. ext4, erofs)*/
	int mode;				/**< file system mount mode. (ro=DISKMOUNT_TYPE_RO/rw=DISKMOUNT_TYPE_RW) */
	int	redundancy;			/**< redundancy mode. (failover=DISKREDUNDANCY_TYPE_FAILOVER/ab=DISKREDUNDANCY_TYPE_AB) */
	char *blockdev[2];		/**< block device for rootfs primary and secondary. */
};
typedef struct s_container_baseconfig_extradisk container_baseconfig_extradisk_t;	/**< typedef for struct s_container_baseconfig_extradisk. */

/**
 * @struct	s_container_baseconfig_lifecycle
 * @brief	The data structure for container lifecycle settings.  It's a part of s_container_baseconfig.
 */
struct s_container_baseconfig_lifecycle {
	char *halt;		/**< Shutdown request signal for guest. */
	char *reboot;	/**< Reboot request signal for guest. */
	int timeout;	/**< Shutdown timeout for guest. */
};
typedef struct s_container_baseconfig_lifecycle container_baseconfig_lifecycle_t;	/**< typedef for struct s_container_baseconfig_lifecycle. */

/**
 * @struct	s_container_baseconfig_capability
 * @brief	The data structure for capabilities setting.  It's a part of s_container_baseconfig.
 */
struct s_container_baseconfig_capability {
	char *drop;	/**< Drop capabilities. */
	char *keep;	/**< Keep capabilities. */
};
typedef struct s_container_baseconfig_capability container_baseconfig_capability_t;	/**< typedef for struct s_container_baseconfig_capability. */

/**
 * @struct	s_container_baseconfig_idmap
 * @brief	The data structure for id mapping to use unprivileged container.  It's a part of s_container_baseconfig_idmaps.
 */
struct s_container_baseconfig_idmap {
	int guest_root_id;	/**< Gest root id. Typically 0. */
	int host_start_id;	/**< ID mapping start point in host. */
	int num_of_id;		/**< Num of id using guest. */
};
typedef struct s_container_baseconfig_idmap container_baseconfig_idmap_t;	/**< typedef for struct s_container_baseconfig_idmap. */

/**
 * @struct	s_container_baseconfig_idmaps
 * @brief	The data structure for id mapping to use unprivileged container.  It's a part of s_container_baseconfig.
 */
struct s_container_baseconfig_idmaps {
	int enabled;						/**< Enable id mapping. */
	container_baseconfig_idmap_t uid;	/**< The uid mapping between host and guest. */
	container_baseconfig_idmap_t gid;	/**< The gid mapping between host and guest. */
};
typedef struct s_container_baseconfig_idmaps container_baseconfig_idmaps_t;	/**< typedef for struct s_container_baseconfig_idmaps. */

/**
 * @struct	s_container_baseconfig_env
 * @brief	The data structure for environment variable setting.  It's a part of s_container_baseconfig.
 */
struct s_container_baseconfig_env {
	struct dl_list list;	/**< Double link list header. */
	char *envstring;		/**< Environment variable list. */
};
typedef struct s_container_baseconfig_env container_baseconfig_env_t;	/**< typedef for struct s_container_baseconfig_env. */

/**
 * @struct	s_container_baseconfig
 * @brief	The data structure for container config base section. It's including basic config for guest container.
 */
struct s_container_baseconfig {
	int	autoboot;								/**< Autoboot setting 1=true, 0=false. When it set 1. container manager launch this guest container at boot time. */
	int bootpriority;							/**< Bootpriority for this guest container, 1 is highest. container manager select launch order using preferential order of guest containers at boot time. */
	container_baseconfig_rootfs_t rootfs;		/**< The data structure for container root filesystem. */
	struct dl_list extradisk_list;				/**< Double link list for s_container_baseconfig_extradisk. */
	container_baseconfig_lifecycle_t lifecycle;	/**< The data structure for container lifecycle settings. */
	container_baseconfig_capability_t cap;		/**< The data structure for capabilities setting. */
	container_baseconfig_idmaps_t idmaps;		/**< The data structure for id mapping to use unprivileged container. */
	struct dl_list envlist;						/**< Double link list for s_container_baseconfig_env. */
	//--- internal control data
	int abboot;									/**< Reserved. */
};
typedef struct s_container_baseconfig container_baseconfig_t;	/**< typedef for struct s_container_baseconfig. */
//-----------------------------------------------------------------------------
// resource config ---------------------------------
/**
 * @def	RESOURCE_TYPE_CGROUP
 * @brief	Resource type is cgroup.  It use at s_container_resource_elem.type.
 */
#define RESOURCE_TYPE_CGROUP	(1)

/**
 * @struct	s_container_resource_elem
 * @brief	The data structure for container resource control setting.  It's a list element for resourcelist of s_container_resource.
 */
struct s_container_resource_elem {
	struct dl_list list;	/**< Double link list header. */
	int type;				/**< resource control type. */
	char *object;			/**< resource object. */
	char *value;			/**< value for resource object. */
};
typedef struct s_container_resource_elem container_resource_elem_t;	/**< typedef for struct s_container_resource_elem. */

/**
 * @struct	s_container_resource
 * @brief	The data structure for container resource control settings.  It's a part of s_container_resourceconfig.
 */
struct s_container_resource {
	struct dl_list resourcelist;	/**< Double link list for s_container_resource_elem.*/
};
typedef struct s_container_resource container_resource_t;	/**< typedef for struct s_container_resource. */

/**
 * @struct	s_container_resourceconfig
 * @brief	The data structure for container resource control settings.  It's including resource control config for guest container.
 */
struct s_container_resourceconfig {
	container_resource_t resource;	/**< The data structure for container resource control settings. */
};
typedef struct s_container_resourceconfig container_resourceconfig_t;	/**< typedef for struct s_container_resourceconfig. */
//-----------------------------------------------------------------------------
// fs config ---------------------------------
/**
 * @def	FSMOUNT_TYPE_FILESYSTEM
 * @brief	File system mount configuration is pseudo filesystem.  Shall not use this config to disk mount.
 */
#define FSMOUNT_TYPE_FILESYSTEM	(1)
/**
 * @def	FSMOUNT_TYPE_DIRECTORY
 * @brief	File system mount configuration is bind mount from host to guest.
 */
#define FSMOUNT_TYPE_DIRECTORY	(2)

/**
 * @struct	s_container_fsmount_elem
 * @brief	The data structure for container filesystem level mount setting.  It's a list element for mountlist of s_container_fsmount.
 */
struct s_container_fsmount_elem {
	struct dl_list list;	/**< Double link list header. */
	int type;				/**< mount type. */
	char *from;				/**< host side mount entry.  */
	char *to;				/**< guest side mount entry.  */
	char *fstype;			/**< file system type. */
	char *option;			/**< mount option.  */
};
typedef struct s_container_fsmount_elem container_fsmount_elem_t;	/**< typedef for struct s_container_fsmount_elem. */

/**
 * @struct	s_container_fsmount
 * @brief	The data structure for container filesystem level mount settings.  It's a part of s_container_fsconfig.
 */
struct s_container_fsmount {
	struct dl_list mountlist;	/**< Double link list for s_container_fsmount_elem.*/
};
typedef struct s_container_fsmount container_fsmount_t;	/**< typedef for struct s_container_fsmount. */

/**
 * @struct	s_container_fsconfig
 * @brief	The data structure for container filesystem level mount settings.  It's including filesystem level mount config for guest container.
 */
struct s_container_fsconfig {
	container_fsmount_t fsmount;	/**< Filesystem level mount management data to manage that. */
};
typedef struct s_container_fsconfig container_fsconfig_t;	/**< typedef for struct s_container_fsconfig. */
//-----------------------------------------------------------------------------
// device config ---------------------------------
/**
 * @def	DEVICE_TYPE_DEVNODE
 * @brief	Device type for device node.  It use in static device setting.
 */
#define DEVICE_TYPE_DEVNODE	(1)
/**
 * @def	DEVICE_TYPE_DEVDIR
 * @brief	Device type for device directory.  It use in static device setting.
 */
#define DEVICE_TYPE_DEVDIR	(2)
/**
 * @def	DEVICE_TYPE_GPIO
 * @brief	Device type for sysfs gpio.  It use in static device setting.
 */
#define DEVICE_TYPE_GPIO	(3)
/**
 * @def	DEVICE_TYPE_IIO
 * @brief	Device type for iio device, sysfs and device node.  It use in static device setting.
 */
#define DEVICE_TYPE_IIO		(4)

/**
 * @def	DEVNODE_TYPE_CHR
 * @brief	Device node type is character device.  It use in static device setting.
 */
#define DEVNODE_TYPE_CHR	(1)
/**
 * @def	DEVNODE_TYPE_BLK
 * @brief	Device node type is block device.  It use in static device setting.
 */
#define DEVNODE_TYPE_BLK	(2)

/**
 * @struct	s_container_static_device_elem
 * @brief	The data structure for static device setting, that use in device node and device dir with character and block device.  It's a list element for static_devlist of s_container_static_device.
 */
struct s_container_static_device_elem {
	struct dl_list list;	/**< Double link list header. */
	int type;				/**< device type. - DEVICE_TYPE_x */
	char *from;				/**< host side mount entry.  */
	char *to;				/**< guest side mount entry.  */
	char *devnode;			/**< Check device node. */
	int optional;			/**< 0 = required, 1 = optional.  */
	int wideallow;			/**< mount type. */
	int exclusive;			/**< exclusive assign to guest. */
	//--- internal control data
	int is_valid;			/**< static device was available. */
	int devtype;			/**< device driver type - DEVNODE_TYPE_x. */
	int major;				/**< major number. */
	int minor;				/**< minor number. */
};
typedef struct s_container_static_device_elem container_static_device_elem_t;	/**< typedef for struct s_container_static_device_elem. */

/**
 * @def	DEVGPIO_DIRECTION_DC
 * @brief	gpio port direction setting is don't care.  It use in only sysfs gpio setting.
 */
#define DEVGPIO_DIRECTION_DC	(0)
/**
 * @def	DEVGPIO_DIRECTION_IN
 * @brief	gpio port direction setting is input.  It use in only sysfs gpio setting.
 */
#define DEVGPIO_DIRECTION_IN	(1)
/**
 * @def	DEVGPIO_DIRECTION_OUT
 * @brief	gpio port direction setting is output and initial value is low.  It use in only sysfs gpio setting.
 */
#define DEVGPIO_DIRECTION_OUT	(2)
/**
 * @def	DEVGPIO_DIRECTION_LOW
 * @brief	gpio port direction setting is output and initial value is low.  It use in only sysfs gpio setting.
 */
#define DEVGPIO_DIRECTION_LOW	(3)
/**
 * @def	DEVGPIO_DIRECTION_HIGH
 * @brief	gpio port direction setting is output and initial value is high.  It use in only sysfs gpio setting.
 */
#define DEVGPIO_DIRECTION_HIGH	(4)

/**
 * Sub function for gpio direction check.
 * If x is DEVGPIO_DIRECTION_DC, DEVGPIO_DIRECTION_IN, DEVGPIO_DIRECTION_OUT, DEVGPIO_DIRECTION_LOW and DEVGPIO_DIRECTION_HIGH, this function return 1 (valid).
 *
 * @param [in]	x	gpio direction mode.
 * @return int
 * @retval  1 x is valid value.
 * @retval -1 x is invalid value.
 */
inline static int devgpio_direction_isvalid(int x) {
	int ret = -1;

	if ((x >= DEVGPIO_DIRECTION_DC) && (x <= DEVGPIO_DIRECTION_HIGH))
		ret = 1;

	return ret;
}

/**
 * @struct	s_container_static_gpio_elem
 * @brief	The data structure for static device setting, that use in gpio device.  It's a list element for static_gpiolist of s_container_static_device.
 */
struct s_container_static_gpio_elem {
	struct dl_list list;	/**< Double link list header. */
	int type;				/**< device type. */
	int port;				/**< gpio port num. */
	int portdirection;		/**< gpio port direction. */
	char *from;				/**< host side mount entry.  */
	char *to;				/**< guest side mount entry.  */
	//--- internal control data
	int is_valid;			/**< gpio port was available. */
};
typedef struct s_container_static_gpio_elem container_static_gpio_elem_t;	/**< typedef for struct s_container_static_gpio_elem. */

/**
 * @struct	s_container_static_iio_elem
 * @brief	The data structure for static device setting, that use in iio device.  It's a list element for static_iiolist of s_container_static_device.
 */
struct s_container_static_iio_elem {
	struct dl_list list;	/**< Double link list header. */
	int type;				/**< device type. */
	char *sysfrom;			/**< host side mount entry sysfs. */
	char *systo;			/**< guest side mount entry sysfs. */
	char *devfrom;			/**< host side mount entry dev. */
	char *devto;			/**< guest side mount entry dev. */
	char *devnode;			/**< Check device node. */
	int optional;			/**< 0 = required, 1 = optional.  */
	//--- internal control data
	int is_sys_valid;		/**< sysfs node was available. */
	int is_dev_valid;		/**< device node was available. */
	int major;				/**< major number. */
	int minor;				/**< minor number. */
};
typedef struct s_container_static_iio_elem container_static_iio_elem_t;	/**< typedef for struct s_container_static_iio_elem. */

/**
 * @struct	s_container_static_device
 * @brief	The data structure for all static device settings.  It's a part of s_container_deviceconfig.
 */
struct s_container_static_device {
	struct dl_list static_devlist;	/**< Double link list for s_container_static_device_elem. */
	struct dl_list static_gpiolist;	/**< Double link list for s_container_static_gpio_elem. */
	struct dl_list static_iiolist;	/**< Double link list for s_container_static_iio_elem. */
};
typedef struct s_container_static_device container_static_device_t;	/**< typedef for struct s_container_static_device. */

/**
 * @struct	s_dynamic_device_elem_data
 * @brief	The data structure for dynamic device management.  It's a list element for device_list of s_container_dynamic_device_elem.  This data assign to s_container_dynamic_device_elem dynamically.
 */
struct s_dynamic_device_elem_data {
	struct dl_list list;	/**< Double link list header. */
	char *devpath;			/**< device path of this device. */
	char *devtype;			/**< device typeof this device. */
	char *subsystem;		/**< subsystem name of this device. */
	char *devnode;			/**< device node name of this device. - char/block device only */
	dev_t devnum;			/**< device major and minor number of this device. - char/block device only. */
	char *diskseq;			/**< diskseq of this device. - block device - disk/partition only. */
	char *partn;			/**< partition num of this device. - block device - partition only. */
	//--- internal control data
	int is_available;		/**< this dynamic device is available. */
};
typedef struct s_dynamic_device_elem_data dynamic_device_elem_data_t;	/**< typedef for struct s_dynamic_device_elem_data. */

/**
 * @struct	s_container_dynamic_device_elem
 * @brief	The data structure for dynamic device setting.  It's a list element for dynamic_devlist of s_container_dynamic_device.  This has dynamic device assignment key.
 */
struct s_container_dynamic_device_elem {
	struct dl_list list;	/**< Double link list header */
	char *devpath;			/**< Assignment device path into this guest container. */
	char *subsystem;		/**< Assignment device subsystem into this guest container. */
	char *devtype;			/**< Assignment device type into this guest container. */
	int mode;				/**< uevent injection mode in this device. */
	//--- internal control data
	struct dl_list device_list;	/**< Double link list for s_dynamic_device_elem_data to manage dynamic device assignment.*/
};
typedef struct s_container_dynamic_device_elem container_dynamic_device_elem_t;	/**< typedef for struct s_container_dynamic_device_elem. */

/**
 * @struct	s_container_static_device
 * @brief	The data structure for all dynamic device settings.  It's a part of s_container_deviceconfig.
 */
struct s_container_dynamic_device {
	struct dl_list dynamic_devlist;	/**< Double link list for s_container_dynamic_device_elem */
};
typedef struct s_container_dynamic_device container_dynamic_device_t;	/**< typedef for struct s_container_dynamic_device. */

/**
 * @struct	s_container_deviceconfig
 * @brief	The data structure for all device management settings.  It's including device management config and current status for guest container.
 */
struct s_container_deviceconfig {
	container_static_device_t static_device;	/**< Static device management data to manage that. */
	container_dynamic_device_t dynamic_device;	/**< Dynamic device management data to manage that. */
};
typedef struct s_container_deviceconfig container_deviceconfig_t;	/**< typedef for struct s_container_deviceconfig. */

//-----------------------------------------------------------------------------
// network interface ---------------------------------
/**
 * @def	STATICNETIF_VETH
 * @brief	Static network interface type is virtual ethernet.  It use in static network interface setting.
 */
#define STATICNETIF_VETH	(1)

/**
 * @struct	s_netif_elem_veth
 * @brief	The data structure for veth setting.  It's assign to s_container_static_netif_elem.setting in case of type is STATICNETIF_VETH.
 */
struct s_netif_elem_veth {
	char *name;		/**< The name of veth. */
	char *link;		/**< Linked host side network interface. */
	char *flags;	/**< Initial flag setting of veth if. up or down. */
	char *hwaddr;	/**< MAC address setting for veth. */
	char *mode;		/**< veth mode. bridge or router. */
	char *address;	/**< Initial ip address setting for veth. - ipv4. */
	char *gateway;	/**< Initial default gateway setting for veth. - ipv4. */
};
typedef struct s_netif_elem_veth netif_elem_veth_t;	/**< typedef for struct s_netif_elem_veth. */

/**
 * @struct	s_container_static_netif_elem
 * @brief	The data structure for static network interface setting.  It's a list element for static_netiflist of s_container_static_netif.
 */
struct s_container_static_netif_elem {
	struct dl_list list;	/**< Double link list header. */
	int type;				/**< Static network interface type. Currently support only STATICNETIF_VETH. */
	void *setting;			/**< Network interface specific setting. Need to cast to real type by type member. */
};
typedef struct s_container_static_netif_elem container_static_netif_elem_t;	/**< typedef for struct s_container_static_netif_elem. */

/**
 * @struct	s_container_static_netif
 * @brief	The data structure for static network interface settings.  It's a part of s_container_netifconfig.
 */
struct s_container_static_netif {
	struct dl_list static_netiflist;	/**< Double link list for s_container_static_netif_elem */
};
typedef struct s_container_static_netif container_static_netif_t;	/**< typedef for struct s_container_static_netif. */

struct s_container_dynamic_netif_elem {
	struct dl_list list;	/**< Double link list header. */
	char *ifname;			/**< Dynamic assignment network interface name into guest container. */
	//--- internal control data
	int ifindex;			/**< Network interface index for this network interface. */
	int is_available;		/**< this network interface is available. */
};
typedef struct s_container_dynamic_netif_elem container_dynamic_netif_elem_t;	/**< typedef for struct s_container_dynamic_netif_elem. */

/**
 * @struct	s_container_dynamic_netif
 * @brief	The data structure for dynamic network interface settings.  It's a part of s_container_netifconfig.
 */
struct s_container_dynamic_netif {
	struct dl_list dynamic_netiflist;	/**< Double link list for s_container_dynamic_netif_elem */
};
typedef struct s_container_dynamic_netif container_dynamic_netif_t;	/**< typedef for struct s_container_dynamic_netif. */

/**
 * @struct	s_container_netifconfig
 * @brief	The data structure for all network interface management settings.  It's including network interface management config and current status for guest container.
 */
struct s_container_netifconfig {
	container_static_netif_t static_netif;		/**< Static network interface data to manage that. */
	container_dynamic_netif_t dynamic_netif;	/**< Dynamic network interface data to manage that. */
};
typedef struct s_container_netifconfig container_netifconfig_t;	/**< typedef for struct s_container_netifconfig. */

//-----------------------------------------------------------------------------
// runtime status ---------------------------------
/**
 * @def	CONTAINER_DISABLE
 * @brief	Container runtime status is disable.  This state assign to inactive guest container.
 */
#define CONTAINER_DISABLE		(0)
/**
 * @def	CONTAINER_NOT_STARTED
 * @brief	Container runtime status is not started.  This state assign to after shutdown guest container or before initial launch.
 */
#define CONTAINER_NOT_STARTED	(1)
/**
 * @def	CONTAINER_STARTED
 * @brief	Container runtime status is started.  This state assign to running guest container.
 */
#define CONTAINER_STARTED		(2)
/**
 * @def	CONTAINER_REBOOT
 * @brief	Container runtime status is now reboot.  This state is assigned to containers that are now operating of reboot.
 */
#define CONTAINER_REBOOT		(3)
/**
 * @def	CONTAINER_SHUTDOWN
 * @brief	Container runtime status is now shutdown.  This state is assigned to containers that are now operating of shutdown..
 */
#define CONTAINER_SHUTDOWN		(4)
/**
 * @def	CONTAINER_DEAD
 * @brief	Container runtime status is dead.  This state assign to exited guest container without reboot or shutdown operation.
 */
#define CONTAINER_DEAD			(5)
/**
 * @def	CONTAINER_EXIT
 * @brief	Container runtime status is exited.  This state assign to exited guest container with shutdown operation.
 */
#define CONTAINER_EXIT			(6)

/**
 * @struct	s_container_runtime_status
 * @brief	The runtime data of this guest container.
 */
struct s_container_runtime_status {
	struct lxc_container *lxc;		/**< Pointer to liblxc container instance. */
	int64_t timeout;				/**< Timeout point of this guest container on shutdown or reboot operation. */
	int status;						/**< Runtime status of this guest container. */
	pid_t pid;						/**< A pid of guest container init process. */
	sd_event_source *pidfd_source;	/**< A pidfd event source for guest container init process. It use guest monitoring. */
};
typedef struct s_container_runtime_status container_runtime_status_t;	/**< typedef for struct s_container_runtime_status. */
//-----------------------------------------------------------------------------
/**
 * @struct	s_container_config
 * @brief	The per container data for container management.
 */
struct s_container_config {
	char *name;									/**< Container name. */
	char *role;									/**< Container role. ex. ivi, cluster. */
	container_baseconfig_t baseconfig;			/**< Base config of this guest container. */
	container_resourceconfig_t resourceconfig;	/**< Resource config of this guest container. */
	container_fsconfig_t fsconfig;				/**< Filesystem config of this guest container. */
	container_deviceconfig_t deviceconfig;		/**< Device config of this guest container. */
	container_netifconfig_t netifconfig;		/**< Network interface config of this guest container. */
	//--- internal control data
	container_runtime_status_t runtime_stat;	/**< Runtime status of this guest container. */
};
typedef struct s_container_config container_config_t;	/**< typedef for struct s_container_config. */
//-----------------------------------------------------------------------------
/**
 * @def	GUEST_CONTAINER_LIMIT
 * @brief	Maximum Number of container guest.
 */
#define GUEST_CONTAINER_LIMIT	(8)

struct s_container_mngsm;
typedef struct s_container_mngsm container_mngsm_t;

struct s_container_control_interface;
typedef struct s_container_control_interface container_control_interface_t;

/**
 * @def	CM_SYSTEM_STATE_RUN
 * @brief	Container manager state is running mode, that state set in booting time.

 */
#define CM_SYSTEM_STATE_RUN			(0)
/**
 * @def	CM_SYSTEM_STATE_SHUTDOWN
 * @brief	Container manager state is shutdown mode, that state was required by system management process.
 */
#define CM_SYSTEM_STATE_SHUTDOWN	(1)

/**
 * @struct	s_containers
 * @brief	The top data structure for container manager.  It’s include all of data for container manager.
 */
struct s_containers {
	container_manager_config_t *cmcfg;	/**< Global config for container manager*/

	int num_of_container;				/**< Num of container data */
	int sys_state;						/**< Container manager state, that is following at system state. */
	container_config_t **containers;	/**< container config array */

	container_mngsm_t *cms;				/**< container management state machine */
	container_control_interface_t *cci;	/**< container control interface */
	dynamic_device_manager_t *ddm;		/**< dynamic device manager */

	sd_event *event;					/**< Systemd event loop object at main loop. */
};
typedef struct s_containers containers_t;	/**< typedef for struct s_containers. */
//-----------------------------------------------------------------------------

#endif //#ifndef CONTAINER_H
