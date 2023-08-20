/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-workqueue.h
 * @brief	Header file for the implementation for per container extra operation.
 */
#ifndef CONTAINER_WORKQUEUE_H
#define CONTAINER_WORKQUEUE_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include "container.h"

//-----------------------------------------------------------------------------
int container_workqueue_cleanup(container_workqueue_t *workqueue, int *after_execute);
int container_workqueue_run(container_workqueue_t *workqueue);
int container_workqueue_remove(container_workqueue_t *workqueue);
int container_workqueue_schedule(container_workqueue_t *workqueue, container_worker_func_t func, int launch_after_end);
int container_workqueue_get_status(container_workqueue_t *workqueue);
int container_workqueue_initialize(container_workqueue_t *workqueue);
int container_workqueue_deinitialize(container_workqueue_t *workqueue);

//-----------------------------------------------------------------------------
#endif //#ifndef CONTAINER_WORKQUEUE_H
