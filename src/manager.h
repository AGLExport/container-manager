/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	manager.h
 * @brief	container management daemon global config data
 */
#ifndef MANAGER_H
#define MANAGER_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include "list.h"

//-----------------------------------------------------------------------------
// Manager config ---------------------------------
/**
 * @struct	s_container_manager_bridge_config
 * @brief	The data structure for network bridge.  It use bridge interface creation at boot time of container manager.
 */
struct s_container_manager_bridge_config {
	struct dl_list list;	/**< Double link list header. */
	char *name;	 			/**< Ethernet bridge name */
	//--- internal control data
};
typedef struct s_container_manager_bridge_config container_manager_bridge_config_t;	/**< typedef for struct s_container_manager_bridge_config. */

/**
 * @def	MANAGER_MOUNT_TYPE_PRE
 * @brief	Block device mount at pre guest launch and unmount at post guest terminate.
 */
#define MANAGER_MOUNT_TYPE_PRE			(1)	//Not support yet.
/**
 * @def	MANAGER_MOUNT_TYPE_POST
 * @brief	Block device mount at post guest launch and unmount at pre guest terminate.
 */
#define MANAGER_MOUNT_TYPE_POST			(2)	//Not support yet.
/**
 * @def	MANAGER_MOUNT_TYPE_DELAYED
 * @brief	Block device mount at post guest launch and unmount at post guest terminate.
 */
#define MANAGER_MOUNT_TYPE_DELAYED		(3)
/**
 * @def	MANAGER_DISKMOUNT_TYPE_RO
 * @brief	Disk mount type is read only. It use at s_container_manager_operation_mount_elem.mode.
 */
#define MANAGER_DISKMOUNT_TYPE_RO	(0)
/**
 * @def	MANAGER_DISKMOUNT_TYPE_RW
 * @brief	Disk mount type is read write. It use at s_container_manager_operation_mount_elem.mode.
 */
#define MANAGER_DISKMOUNT_TYPE_RW	(1)
/**
 * @def	MANAGER_DISKREDUNDANCY_TYPE_FAILOVER
 * @brief	Disk mount redundancy type is fail over (1st disk mount was fail, try to mount 2nd disk). It use at s_container_manager_operation_mount_elem.redundancy.
 */
#define MANAGER_DISKREDUNDANCY_TYPE_FAILOVER	(0)
/**
 * @def	MANAGER_DISKREDUNDANCY_TYPE_AB
 * @brief	Disk mount redundancy type is AB (Automatically select A or B depend on boot parameter). It use at s_container_manager_operation_mount_elem.redundancy.
 */
#define MANAGER_DISKREDUNDANCY_TYPE_AB			(1)
/**
 * @def	MANAGER_DISKREDUNDANCY_TYPE_AB
 * @brief	Disk mount redundancy type is fsck (Automatically run file system check in case of mount fail). It use at s_container_manager_operation_mount_elem.redundancy.
 */
#define MANAGER_DISKREDUNDANCY_TYPE_FSCK		(2)
/**
 * @def	MANAGER_DISKREDUNDANCY_TYPE_AB
 * @brief	Disk mount redundancy type is AB (Automatically run mkfs in case of mount fail). It use at s_container_manager_operation_mount_elem.redundancy.
 */
#define MANAGER_DISKREDUNDANCY_TYPE_MKFS		(3)
/**
 * @struct	s_container_manager_operation_mount_elem
 * @brief	The data structure for manager mount disk.  It's a list element for mount_list of s_container_manager_operation_mount.
 */
/**
 * @def	MANAGER_WORKER_STATE_NOP
 * @brief	.
 */
#define MANAGER_WORKER_STATE_NOP		(0)
/**
 * @def	MANAGER_WORKER_STATE_DO
 * @brief	.
 */
#define MANAGER_WORKER_STATE_QUEUED		(1)
/**
 * @def	MANAGER_WORKER_STATE_COMPLETE
 * @brief	.
 */
#define MANAGER_WORKER_STATE_COMPLETE	(2)
/**
 * @def	MANAGER_WORKER_STATE_CANCELED
 * @brief	.
 */
#define MANAGER_WORKER_STATE_CANCELED	(3)
/**
 * @struct	s_container_manager_operation_mount_elem
 * @brief	The data structure for manager mount disk.  It's a list element for mount_list of s_container_manager_operation_mount.
 */
struct s_container_manager_operation_mount_elem {
	struct dl_list list;	/**< Double link list header. */
	int type;				/**< mount operation type MANAGER_MOUNT_TYPE_PRE or MANAGER_MOUNT_TYPE_DIRECTORY or MANAGER_MOUNT_TYPE_DELAYED. */
	char *to;				/**< mount path in host. */
	char *filesystem;		/**< file system type. (ex. ext4, erofs)*/
	int mode;				/**< file system mount mode. (ro=DISKMOUNT_TYPE_RO/rw=DISKMOUNT_TYPE_RW) */
	char *option;			/**< file system specific mount option. (ex. data=ordered,errors=remount-ro at ext4)*/
	int	redundancy;			/**< redundancy mode. (failover=DISKREDUNDANCY_TYPE_FAILOVER/ab=DISKREDUNDANCY_TYPE_AB) */
	char *blockdev[2];		/**< block device for rootfs primary and secondary. */
	//--- internal control data
	int index;				/**< Internal index.*/
	int is_mounted;			/**< This extra disk is mounted or not. 0: not mounted. 1: mounted.*/
	int is_dispatched;		/**< Already dispatched worker 0:no 1: yes.*/
	int error_count;		/**< mount error count of this extra disk. That exclude busy error.*/
	int state;				/**< State of this worker.*/
};
typedef struct s_container_manager_operation_mount_elem container_manager_operation_mount_elem_t;	/**< typedef for struct s_container_manager_operation_mound_elem. */

/**
 * @struct	s_container_manager_operation_mount
 * @brief	The data structure for manager mount disks.
 */
struct s_container_manager_operation_mount {
	struct dl_list mount_list;	/**< Double link list header. */
	//--- internal control data
};
typedef struct s_container_manager_operation_mount container_manager_operation_mount_t;	/**< typedef for struct s_container_manager_operation_mount. */

/**
 * @struct	s_container_manager_operation_storage
 * @brief	The data storage for manager operations.
 */
struct s_container_manager_operation_storage;
typedef struct s_container_manager_operation_storage container_manager_operation_storage_t;	/**< typedef for struct s_container_manager_operation_storage. */

/**
 * @struct	s_container_manager_operation
 * @brief	The data structure for manager operation.
 */
struct s_container_manager_operation {
	container_manager_operation_mount_t mount;		/**< Mount operation. */
	//--- internal control data
	container_manager_operation_storage_t *storage;
};
typedef struct s_container_manager_operation container_manager_operation_t;	/**< typedef for struct s_container_manager_operation. */

struct s_container_config;
typedef struct s_container_config container_config_t;

/**
 * @struct	s_container_manager_role_elem
 * @brief	The data structure for container role management.  This data use to manage which guest container has which role.
 */
struct s_container_manager_role_elem {
	struct dl_list list;	/**< Double link list header. */
	container_config_t *cc;	/**< pointer to container config */
	//--- internal control data
};
typedef struct s_container_manager_role_elem container_manager_role_elem_t;	/**< typedef for struct s_container_manager_role_elem. */

/**
 * @struct	s_container_manager_role_config
 * @brief	The data structure for container role management.  This data show role.  The container_list of this is linking all guest container that has same as own role.
 */
struct s_container_manager_role_config {
	struct dl_list list;			/**< Double link list header. */
	char *name;	 					/**< Role name. */
	//--- internal control data
	struct dl_list container_list;	/**< Double link list for struct s_container_config . */
};
typedef struct s_container_manager_role_config container_manager_role_config_t;	/**< typedef for struct s_container_manager_role_config. */

/**
 * @struct	s_container_manager_config
 * @brief	Top level data for container manager config.
 */
struct s_container_manager_config {
	char *configdir;			/**< Guest container config directory */
	struct dl_list bridgelist;	/**< Double link list for s_container_manager_bridge_config. */
	container_manager_operation_t operation;	/**< The manager operations. */
	//--- internal control data
	struct dl_list role_list;	/**< Double link list for s_container_manager_role_config. */
};
typedef struct s_container_manager_config container_manager_config_t;	/**< typedef for struct s_container_manager_config. */

//-----------------------------------------------------------------------------
#endif //#ifndef MANAGER_H
