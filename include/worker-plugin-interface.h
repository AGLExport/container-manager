
/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	worker-plugin-interface.h
 * @brief	Interface header for worker plugin development
 */
#ifndef WORKER_PLUGIN_INTERFACE_H
#define WORKER_PLUGIN_INTERFACE_H

/**
 * @def	RESOURCE_TYPE_CGROUP
 * @brief	Resource type is cgroup.  It use at s_container_resource_elem.type.
 */
typedef void* cm_worker_handle_t;

/**
 * @brief Function pointer for set argument to container workqueue worker.
 *
 * @param [in]	argc	An integer argument count of the command line arguments.
 * @param [in]	argv	An argument vector of the command line arguments.
 * @return Description for return value
 * @retval 0	Success to set args.
 * @retval -1	Fail to set args.
 */
typedef int (*cm_worker_set_args_t)(cm_worker_handle_t handle, const char *arg_str, int arg_length);

/**
 * @brief Function pointer for execute to container workqueue worker.
 *
 * @return Description for return value
 * @retval 0	Success to execute worker.
 * @retval -1	Fail to execute worker.
 */
typedef int (*cm_worker_exec_t)(cm_worker_handle_t handle);

/**
 * @brief Function pointer for cancel to container workqueue worker.
 *
 * @return Description for return value
 * @retval 0	Success to cancel request to worker.
 * @retval -1	Fail to cancel request to worker.
 */
typedef int (*cm_worker_cancel_t)(cm_worker_handle_t handle);

/**
 * @struct	s_cm_worker_instance
 * @brief	The data structure for container manager workqueue worker instance.
 */
struct s_cm_worker_instance {
    cm_worker_handle_t handle;      /**< A handle for cm worker instance. */
    cm_worker_set_args_t set_args;  /**< A function pointer for cm_worker_set_args_t. */
    cm_worker_exec_t exec;          /**< A function pointer for cm_worker_exec_t. */
    cm_worker_cancel_t cancel;      /**< A function pointer for cm_worker_cancel_t. */
};
typedef struct s_cm_worker_instance cm_worker_instance_t;	/**< typedef for struct s_cm_worker_instance. */

/**
 * @brief Entry point for the container workqueue worker plugin.
 *
 * @param [in]	argc	An integer argument count of the command line arguments.
 * @param [in]	argv	An argument vector of the command line arguments.
 * @return Description for return value
 * @retval 0	Success to execute worker.
 * @retval -1	Fail to execute worker.
 */
int __attribute__((visibility ("default"))) cm_worker_new(cm_worker_instance_t **instance);
typedef int (*cm_worker_new_t)(cm_worker_instance_t **instance);

/**
 * @brief Entry point for the container workqueue worker plugin.
 *
 * @param [in]	argc	An integer argument count of the command line arguments.
 * @param [in]	argv	An argument vector of the command line arguments.
 * @return Description for return value
 * @retval 0	Success to execute worker.
 * @retval -1	Fail to execute worker.
 */
int __attribute__((visibility ("default"))) cm_worker_delete(cm_worker_instance_t *instance);
typedef int (*cm_worker_delete_t)(cm_worker_instance_t *instance);
//-----------------------------------------------------------------------------
#endif //#ifndef WORKER_PLUGIN_INTERFACE_H