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

int manager_operation_delayed_launch(containers_t *cs);
int manager_operation_delayed_poll(containers_t *cs);
int manager_operation_delayed_terminate(containers_t *cs);

//-----------------------------------------------------------------------------
#endif //#ifndef CONTAINER_MANAGER_OPERATIONS_H

