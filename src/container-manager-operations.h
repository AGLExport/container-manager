/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-manager-operations.h
 * @brief	A header file for manager operation.
 */

#ifndef CONTAINER_MANAGER_OPERATIONS_H
#define CONTAINER_MANAGER_OPERATIONS_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include "manager.h"

//-----------------------------------------------------------------------------
/**
 * @def	MANAGER_WORKER_OPERATION_TYPE_START
 * @brief	The operation type for the start time.
 */
#define MANAGER_WORKER_OPERATION_TYPE_START				(1U << 0)
/**
 * @def	MANAGER_WORKER_OPERATION_TYPE_TERMINATE
 * @brief	The operation type for the terminate time.
 */
#define MANAGER_WORKER_OPERATION_TYPE_TERMINATE			(1U << 1)
/**
 * @def	MANAGER_WORKER_OPERATION_TYPE_TERMINATE_EXT
 * @brief	The operation type for the extended terminate time.
 */
#define MANAGER_WORKER_OPERATION_TYPE_TERMINATE_EXT		(1U << 2)


int manager_operation_delayed_launch(containers_t *cs, unsigned int op_type);
int manager_operation_delayed_poll(containers_t *cs);

//-----------------------------------------------------------------------------
#endif //#ifndef CONTAINER_MANAGER_OPERATIONS_H

